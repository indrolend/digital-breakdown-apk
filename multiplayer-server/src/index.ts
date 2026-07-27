import { DurableObject } from "cloudflare:workers";
import { MAX_MESSAGE_BYTES, MAX_PLAYERS, PROTOCOL_VERSION, isRoomCode, jsonResponse, makeRoomCode, parseBinaryHeader, parseEnvelope } from "./protocol";

const SERVICE_NAME = "digital-breakdown-multiplayer";

function deploymentIdentity(env: Env): Record<string, unknown> {
  const environment = typeof env.ENVIRONMENT === "string" && env.ENVIRONMENT.length > 0 ? env.ENVIRONMENT : "local";
  const metadata = env.CF_VERSION_METADATA;
  const commit = typeof env.DEPLOY_COMMIT === "string" && env.DEPLOY_COMMIT.length > 0 ? env.DEPLOY_COMMIT : metadata?.id ?? "local";
  const deployedAt = typeof env.DEPLOYED_AT === "string" && env.DEPLOYED_AT.length > 0 ? env.DEPLOYED_AT : metadata?.timestamp ?? "local";
  return {
    status: "ok",
    ok: true,
    service: SERVICE_NAME,
    environment,
    protocolVersion: PROTOCOL_VERSION,
    protocol: PROTOCOL_VERSION,
    commit,
    workerVersion: metadata?.id ?? "local",
    deployedAt,
  };
}

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
const MATCH_CAPACITY = 2;

interface MatchLifecycle {
  started: boolean;
  startId: number;
}

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

  private async lifecycle(): Promise<MatchLifecycle> {
    return (await this.ctx.storage.get<MatchLifecycle>("match_lifecycle")) ??
      { started: false, startId: 0 };
  }

  private lobbyState(lifecycle: MatchLifecycle): Record<string, unknown> {
    const players = this.sockets().map(({ attachment }) => attachment.playerId).sort();
    return {
      type: "lobby_state",
      protocol: PROTOCOL_VERSION,
      players,
      playerCount: players.length,
      capacity: MATCH_CAPACITY,
      state: lifecycle.started ? "started" : "lobby",
      started: lifecycle.started,
      startId: lifecycle.startId,
    };
  }

  private async broadcastLobby(): Promise<void> {
    this.broadcast(this.lobbyState(await this.lifecycle()));
  }

  private send(socket: WebSocket, value: unknown): void {
    try { socket.send(JSON.stringify(value)); } catch { socket.close(1011, "send_failed"); }
  }

  private broadcast(value: unknown, exceptPlayer = -1): void {
    const encoded = JSON.stringify(value);
    for (const { socket, attachment } of this.sockets()) {
      if (attachment.playerId !== exceptPlayer) {
        try { socket.send(encoded); } catch { socket.close(1011, "send_failed"); }
      }
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
    const lifecycle = await this.lifecycle();
    if (role === "host") {
      if (connected.some(({ attachment }) => attachment.role === "host")) return jsonResponse({ error: "host_already_connected" }, 409);
      const key = url.searchParams.get("key") ?? "";
      if ((await sha256(key)) !== metadata.hostKeyHash) return jsonResponse({ error: "invalid_host_key" }, 403);
    } else if (lifecycle.started || connected.length >= MATCH_CAPACITY || !connected.some(({ attachment }) => attachment.role === "host")) {
      return jsonResponse({ error: lifecycle.started ? "match_started" : connected.length >= MATCH_CAPACITY ? "room_full" : "host_offline" }, 409);
    }

    const used = new Set(connected.map(({ attachment }) => attachment.playerId));
    const playerId = role === "host" ? 0 : [1].find((id) => !used.has(id));
    if (playerId === undefined) return jsonResponse({ error: "room_full" }, 409);
    const pair = new WebSocketPair();
    const client = pair[0];
    const server = pair[1];
    this.ctx.acceptWebSocket(server);
    server.serializeAttachment({ playerId, role, build } satisfies SocketAttachment);
    this.send(server, { type: "welcome", protocol: PROTOCOL_VERSION, gameplayVersion, room: metadata.code, playerId, role, players: [...used, playerId].sort() });
    this.broadcast({ type: role === "host" ? "host_reconnected" : "player_joined", playerId, build }, playerId);
    await this.broadcastLobby();
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
        try { peer.socket.send(message); } catch { peer.socket.close(1011, "send_failed"); }
      }
      return;
    }
    let control: Record<string, unknown> | null = null;
    try {
      const parsed: unknown = JSON.parse(message);
      if (parsed && typeof parsed === "object") control = parsed as Record<string, unknown>;
    } catch { /* handled by parseEnvelope below */ }
    const controlType = typeof control?.type === "string" ? control.type : "";
    if (controlType === "heartbeat") {
      this.send(socket, { type: "heartbeat_ack", sentAt: Number(control?.sentAt) || 0 });
      return;
    }
    if (controlType === "lobby_ready") {
      await this.broadcastLobby();
      return;
    }
    if (controlType === "start_match") {
      const lifecycle = await this.lifecycle();
      if (attachment.role !== "host") {
        this.send(socket, { type: "error", code: "host_only" });
        return;
      }
      if (lifecycle.started) {
        this.send(socket, { type: "error", code: "already_started" });
        return;
      }
      if (this.sockets().length !== MATCH_CAPACITY) {
        this.send(socket, { type: "error", code: "waiting_for_player" });
        return;
      }
      const startId = Number(control?.startId);
      const gameplayVersion = Number(control?.gameplayVersion);
      if (!Number.isSafeInteger(startId) || startId <= 0 || gameplayVersion !== this.metadata()?.gameplayVersion) {
        this.send(socket, { type: "error", code: "invalid_start" });
        return;
      }
      await this.ctx.storage.put("match_lifecycle", { started: true, startId } satisfies MatchLifecycle);
      this.broadcast({
        type: "start_match",
        startId,
        gameplayVersion,
        roomSeed: Number(control?.roomSeed) || 1,
        roomIndex: Number(control?.roomIndex) || 0,
        startTick: Number(control?.startTick) || 0,
      });
      return;
    }
    if (controlType === "start_ack") {
      if (attachment.role !== "guest") {
        this.send(socket, { type: "error", code: "guest_only" });
        return;
      }
      const lifecycle = await this.lifecycle();
      const startId = Number(control?.startId);
      if (!lifecycle.started || startId !== lifecycle.startId) {
        this.send(socket, { type: "error", code: "invalid_start_ack" });
        return;
      }
      this.broadcast({
        type: "start_ack",
        startId,
        snapshotSequence: Number(control?.snapshotSequence) || 0,
        playerId: attachment.playerId,
      }, attachment.playerId);
      return;
    }
    if (controlType === "start_confirm") {
      const lifecycle = await this.lifecycle();
      if (attachment.role !== "host" || Number(control?.startId) !== lifecycle.startId) {
        this.send(socket, { type: "error", code: "invalid_start_confirm" });
        return;
      }
      this.broadcast({ type: "start_confirm", startId: lifecycle.startId });
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
      try { peer.socket.send(outbound); } catch { peer.socket.close(1011, "send_failed"); }
    }
  }

  override async webSocketClose(socket: WebSocket, code: number, reason: string): Promise<void> {
    const attachment = socket.deserializeAttachment() as SocketAttachment | null;
    if (!attachment) return;
    if (attachment.role === "host") {
      if (code === 1000 && reason === "leaving") this.closeMatch("host_left");
      else this.broadcast({ type: "host_disconnected", reason: reason || "connection_lost" }, attachment.playerId);
    }
    else {
      this.broadcast({ type: "player_left", playerId: attachment.playerId, code, reason }, attachment.playerId);
      await this.broadcastLobby();
    }
  }

  override async webSocketError(socket: WebSocket): Promise<void> {
    const attachment = socket.deserializeAttachment() as SocketAttachment | null;
    if (attachment?.role === "host") this.broadcast({ type: "host_disconnected", reason: "connection_error" }, attachment.playerId);
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
      if (request.method === "GET" && url.pathname === "/health") return jsonResponse(deploymentIdentity(env));
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
