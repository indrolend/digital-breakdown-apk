import { DurableObject } from "cloudflare:workers";
import { MAX_MESSAGE_BYTES, MAX_PLAYERS, PROTOCOL_VERSION, isRoomCode, jsonResponse, makeRoomCode, parseBinaryHeader, parseEnvelope } from "./protocol";

interface SocketAttachment {
  playerId: number;
  role: "host" | "guest";
  build: string;
}

interface RoomMetadata {
  code: string;
  hostKeyHash: string;
  gameplayVersion: number;
}

const encoder = new TextEncoder();
const ROOM_LIFETIME_MS = 6 * 60 * 60 * 1000;

async function sha256(value: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", encoder.encode(value));
  return [...new Uint8Array(digest)].map((byte) => byte.toString(16).padStart(2, "0")).join("");
}

function secureToken(byteCount = 24): string {
  const bytes = new Uint8Array(byteCount);
  crypto.getRandomValues(bytes);
  return [...bytes].map((byte) => byte.toString(16).padStart(2, "0")).join("");
}

export class MatchRoom extends DurableObject<Env> {
  constructor(ctx: DurableObjectState, env: Env) {
    super(ctx, env);
    ctx.blockConcurrencyWhile(async () => {
      this.ctx.storage.sql.exec(`
        CREATE TABLE IF NOT EXISTS room_metadata (
          singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
          code TEXT NOT NULL,
          host_key_hash TEXT NOT NULL,
          gameplay_version INTEGER NOT NULL,
          created_at INTEGER NOT NULL
        );
      `);
    });
  }

  async initialize(code: string, hostKeyHash: string, gameplayVersion: number): Promise<boolean> {
    const existing = this.metadata();
    if (existing) return false;
    this.ctx.storage.sql.exec(
      "INSERT INTO room_metadata(singleton, code, host_key_hash, gameplay_version, created_at) VALUES(1, ?, ?, ?, ?)",
      code, hostKeyHash, gameplayVersion, Date.now(),
    );
    await this.ctx.storage.setAlarm(Date.now() + ROOM_LIFETIME_MS);
    return true;
  }

  private metadata(): RoomMetadata | null {
    const rows = this.ctx.storage.sql.exec<{ code: string; host_key_hash: string; gameplay_version: number }>(
      "SELECT code, host_key_hash, gameplay_version FROM room_metadata WHERE singleton = 1",
    ).toArray();
    const row = rows[0];
    return row ? { code: row.code, hostKeyHash: row.host_key_hash, gameplayVersion: row.gameplay_version } : null;
  }

  private sockets(): Array<{ socket: WebSocket; attachment: SocketAttachment }> {
    return this.ctx.getWebSockets().flatMap((socket) => {
      const attachment = socket.deserializeAttachment() as SocketAttachment | null;
      return attachment ? [{ socket, attachment }] : [];
    });
  }

  private send(socket: WebSocket, value: unknown): void {
    socket.send(JSON.stringify(value));
  }

  private broadcast(value: unknown, exceptPlayer = -1): void {
    const encoded = JSON.stringify(value);
    for (const { socket, attachment } of this.sockets()) {
      if (attachment.playerId !== exceptPlayer) socket.send(encoded);
    }
  }

  private closeMatch(reason: string): void {
    this.broadcast({ type: "match_closed", reason });
    for (const { socket } of this.sockets()) socket.close(4001, reason);
  }

  override async fetch(request: Request): Promise<Response> {
    if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket") return jsonResponse({ error: "websocket_required" }, 426);
    const metadata = this.metadata();
    if (!metadata) return jsonResponse({ error: "room_not_found" }, 404);
    const url = new URL(request.url);
    const role = url.searchParams.get("role");
    const build = (url.searchParams.get("build") ?? "").slice(0, 64);
    const gameplayVersion = Number(url.searchParams.get("gameplay"));
    if (role !== "host" && role !== "guest") return jsonResponse({ error: "invalid_role" }, 400);
    if (!build || gameplayVersion !== metadata.gameplayVersion) return jsonResponse({ error: "incompatible_build" }, 409);

    const connected = this.sockets();
    if (role === "host") {
      if (connected.some(({ attachment }) => attachment.role === "host")) return jsonResponse({ error: "host_already_connected" }, 409);
      const key = url.searchParams.get("key") ?? "";
      if ((await sha256(key)) !== metadata.hostKeyHash) return jsonResponse({ error: "invalid_host_key" }, 403);
    } else if (connected.length >= MAX_PLAYERS || !connected.some(({ attachment }) => attachment.role === "host")) {
      return jsonResponse({ error: connected.length >= MAX_PLAYERS ? "room_full" : "host_offline" }, 409);
    }

    const used = new Set(connected.map(({ attachment }) => attachment.playerId));
    const playerId = role === "host" ? 0 : [1, 2, 3].find((id) => !used.has(id));
    if (playerId === undefined) return jsonResponse({ error: "room_full" }, 409);
    const pair = new WebSocketPair();
    const client = pair[0];
    const server = pair[1];
    this.ctx.acceptWebSocket(server);
    server.serializeAttachment({ playerId, role, build } satisfies SocketAttachment);
    this.send(server, { type: "welcome", protocol: PROTOCOL_VERSION, gameplayVersion, room: metadata.code, playerId, role, players: [...used, playerId].sort() });
    this.broadcast({ type: "player_joined", playerId, build }, playerId);
    return new Response(null, { status: 101, webSocket: client });
  }

  override async webSocketMessage(socket: WebSocket, message: string | ArrayBuffer): Promise<void> {
    const attachment = socket.deserializeAttachment() as SocketAttachment | null;
    if (!attachment) { socket.close(4002, "missing_session"); return; }
    if (typeof message !== "string") {
      const header = parseBinaryHeader(message);
      if (!header) { socket.close(message.byteLength > MAX_MESSAGE_BYTES ? 4003 : 4005, message.byteLength > MAX_MESSAGE_BYTES ? "message_too_large" : "invalid_message"); return; }
      if (header.playerId !== attachment.playerId) { socket.close(4007, "player_id_spoof"); return; }
      const allowed = attachment.role === "host" ? header.type !== 1 : header.type === 1 || header.type === 4 || header.type === 5;
      if (!allowed) { socket.close(4006, "message_not_allowed"); return; }
      for (const peer of this.sockets()) {
        if (peer.attachment.playerId === attachment.playerId) continue;
        if (attachment.role === "guest" && peer.attachment.role !== "host") continue;
        peer.socket.send(message);
      }
      return;
    }
    const envelope = parseEnvelope(message);
    if (!envelope) { socket.close(4005, "invalid_message"); return; }
    const allowed = attachment.role === "host"
      ? envelope.type !== "input"
      : envelope.type === "input" || envelope.type === "ping" || envelope.type === "pong";
    if (!allowed) { socket.close(4006, "message_not_allowed"); return; }
    const outbound = JSON.stringify({ ...envelope, playerId: attachment.playerId });
    for (const peer of this.sockets()) {
      if (peer.attachment.playerId === attachment.playerId) continue;
      if (attachment.role === "guest" && peer.attachment.role !== "host") continue;
      peer.socket.send(outbound);
    }
  }

  override async webSocketClose(socket: WebSocket, code: number, reason: string): Promise<void> {
    const attachment = socket.deserializeAttachment() as SocketAttachment | null;
    if (!attachment) return;
    if (attachment.role === "host") this.closeMatch("host_left");
    else this.broadcast({ type: "player_left", playerId: attachment.playerId, code, reason }, attachment.playerId);
  }

  override async webSocketError(socket: WebSocket): Promise<void> {
    const attachment = socket.deserializeAttachment() as SocketAttachment | null;
    if (attachment?.role === "host") this.closeMatch("host_connection_error");
  }

  override async alarm(): Promise<void> {
    this.closeMatch("room_expired");
    await this.ctx.storage.deleteAll();
  }
}

async function createRoom(env: Env, request: Request): Promise<Response> {
  let gameplayVersion = 1;
  try {
    const length = Number(request.headers.get("Content-Length") ?? "0");
    if (length > 1024) return jsonResponse({ error: "request_too_large" }, 413);
    const body: unknown = length > 0 ? await request.json() : {};
    if (body && typeof body === "object" && "gameplayVersion" in body) gameplayVersion = Number((body as Record<string, unknown>).gameplayVersion);
  } catch { return jsonResponse({ error: "invalid_json" }, 400); }
  if (!Number.isSafeInteger(gameplayVersion) || gameplayVersion < 1) return jsonResponse({ error: "invalid_gameplay_version" }, 400);
  for (let attempt = 0; attempt < 8; ++attempt) {
    const random = new Uint8Array(6); crypto.getRandomValues(random);
    const code = makeRoomCode(random);
    const hostKey = secureToken();
    const room = env.MATCH_ROOMS.getByName(code);
    if (await room.initialize(code, await sha256(hostKey), gameplayVersion)) {
      return jsonResponse({ protocol: PROTOCOL_VERSION, gameplayVersion, code, hostKey }, 201);
    }
  }
  return jsonResponse({ error: "room_code_exhausted" }, 503);
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    try {
      if (request.method === "GET" && url.pathname === "/health") return jsonResponse({ ok: true, protocol: PROTOCOL_VERSION });
      if (request.method === "POST" && url.pathname === "/v1/rooms") return await createRoom(env, request);
      const match = url.pathname.match(/^\/v1\/rooms\/([A-Z2-9]{6})\/connect$/);
      if (request.method === "GET" && match) {
        const code = match[1]!;
        if (!isRoomCode(code)) return jsonResponse({ error: "invalid_room_code" }, 400);
        return await env.MATCH_ROOMS.getByName(code).fetch(request);
      }
      return jsonResponse({ error: "not_found" }, 404);
    } catch (error) {
      console.error(JSON.stringify({ message: "request_failed", path: url.pathname, error: error instanceof Error ? error.message : String(error) }));
      return jsonResponse({ error: "internal_error" }, 500);
    }
  },
} satisfies ExportedHandler<Env>;
