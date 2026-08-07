import { env, exports } from "cloudflare:workers";
import { runInDurableObject } from "cloudflare:test";
import { describe, expect, it } from "vitest";
import { MatchRoom } from "../src/index";
import { BINARY_HEADER_BYTES, BINARY_MAGIC, PROTOCOL_VERSION } from "../src/protocol";

interface Inbox {
  queue: MessageEvent[];
  waiters: Array<(event: MessageEvent) => void>;
}
const inboxes = new WeakMap<WebSocket, Inbox>();
const PEER_SILENCE_TIMEOUT_MS = MatchRoom.peerSilenceTimeoutMs;

function inbox(socket: WebSocket): Inbox {
  let value = inboxes.get(socket);
  if (value) return value;
  value = { queue: [], waiters: [] };
  inboxes.set(socket, value);
  socket.addEventListener("message", (event) => {
    const waiter = value!.waiters.shift();
    if (waiter) waiter(event);
    else value!.queue.push(event);
  });
  return value;
}

function nextMessage(socket: WebSocket, timeoutMs = 2000): Promise<MessageEvent> {
  const messages = inbox(socket);
  const queued = messages.queue.shift();
  if (queued) return Promise.resolve(queued);
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("websocket message timeout")), timeoutMs);
    messages.waiters.push((event) => {
      clearTimeout(timer);
      resolve(event);
    });
  });
}

async function nextJsonType(socket: WebSocket, type: string): Promise<Record<string, unknown>> {
  for (let attempt = 0; attempt < 8; ++attempt) {
    const event = await nextMessage(socket);
    if (typeof event.data !== "string") continue;
    const value = JSON.parse(event.data) as Record<string, unknown>;
    if (value.type === type) return value;
  }
  throw new Error(`did not receive ${type}`);
}

function packet(type: number, playerId: number, sequence: number): ArrayBuffer {
  const value = new ArrayBuffer(BINARY_HEADER_BYTES + 1);
  const view = new DataView(value);
  view.setUint32(0, BINARY_MAGIC, true);
  view.setUint16(4, PROTOCOL_VERSION, true);
  view.setUint8(6, type);
  view.setUint8(7, playerId);
  view.setUint32(8, sequence, true);
  view.setUint32(12, sequence, true);
  view.setUint32(16, 1, true);
  view.setUint8(BINARY_HEADER_BYTES, 42);
  return value;
}

async function connect(path: string): Promise<WebSocket> {
  const response = await exports.default.fetch(new Request(`http://local.test${path}`, {
    headers: { Upgrade: "websocket" },
  }));
  expect(response.status).toBe(101);
  expect(response.webSocket).toBeTruthy();
  const socket = response.webSocket!;
  socket.binaryType = "arraybuffer";
  socket.accept();
  inbox(socket);
  return socket;
}

async function expirePeers(code: string, now: number): Promise<void> {
  const stub = env.MATCH_ROOMS.getByName(code);
  await runInDurableObject(stub, async (instance) => {
    await (instance as unknown as { expireSilentPeers(now: number): Promise<void> }).expireSilentPeers(now);
  });
}

async function setPeerActivity(code: string, hostAt: number, guestAt: number): Promise<void> {
  const stub = env.MATCH_ROOMS.getByName(code);
  await runInDurableObject(stub, (_instance, state) => {
    for (const socket of state.getWebSockets()) {
      const attachment = socket.deserializeAttachment() as {
        playerId: number; role: "host" | "guest"; build: string; lastActivityAt: number;
      };
      socket.serializeAttachment({
        ...attachment,
        lastActivityAt: attachment.role === "host" ? hostAt : guestAt,
      });
    }
  });
}

async function peerActivity(code: string, role: "host" | "guest"): Promise<number> {
  return runInDurableObject(env.MATCH_ROOMS.getByName(code), (_instance, state) => {
    for (const socket of state.getWebSockets()) {
      const attachment = socket.deserializeAttachment() as {
        role: "host" | "guest"; lastActivityAt: number;
      };
      if (attachment.role === role) return attachment.lastActivityAt;
    }
    throw new Error(`missing ${role} socket`);
  });
}

describe("room relay integration", () => {
  it("creates, joins, relays both directions, and reports disconnect", async () => {
    const created = await exports.default.fetch(new Request("http://local.test/v1/rooms", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ gameplayVersion: 5 }),
    }));
    expect(created.status).toBe(201);
    const room = await created.json() as { code: string; hostKey: string; protocol: number };
    expect(room.protocol).toBe(PROTOCOL_VERSION);

    const host = await connect(`/v1/rooms/${room.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(room.hostKey)}`);
    const hostWelcome = await nextJsonType(host, "welcome");
    expect(hostWelcome).toMatchObject({ type: "welcome", playerId: 0, role: "host", room: room.code });
    expect(await nextJsonType(host, "lobby_state")).toMatchObject({ playerCount: 1, capacity: 2, started: false });

    const guest = await connect(`/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`);
    const guestWelcome = await nextJsonType(guest, "welcome");
    expect(guestWelcome).toMatchObject({ type: "welcome", playerId: 1, role: "guest", room: room.code });
    expect(await nextJsonType(host, "player_joined")).toMatchObject({ playerId: 1 });
    expect(await nextJsonType(host, "lobby_state")).toMatchObject({ playerCount: 2 });
    expect(await nextJsonType(guest, "lobby_state")).toMatchObject({ playerCount: 2 });

    host.send(JSON.stringify({ type: "start_match", startId: 7, gameplayVersion: 5, roomSeed: 41, roomIndex: 3, startTick: 10 }));
    expect(await nextJsonType(host, "start_match")).toMatchObject({ startId: 7, roomSeed: 41, roomIndex: 3 });
    expect(await nextJsonType(guest, "start_match")).toMatchObject({ startId: 7 });

    const guestInput = packet(1, 1, 1);
    const inputAtHost = nextMessage(host);
    guest.send(guestInput);
    expect((await inputAtHost).data).toBeInstanceOf(ArrayBuffer);

    const hostSnapshot = packet(2, 0, 2);
    const snapshotAtGuest = nextMessage(guest);
    host.send(hostSnapshot);
    expect((await snapshotAtGuest).data).toBeInstanceOf(ArrayBuffer);

    guest.send(JSON.stringify({ type: "start_ack", startId: 7, snapshotSequence: 2 }));
    expect(await nextJsonType(host, "start_ack")).toMatchObject({ startId: 7, snapshotSequence: 2, playerId: 1 });
    host.send(JSON.stringify({ type: "start_confirm", startId: 7 }));
    expect(await nextJsonType(host, "start_confirm")).toMatchObject({ startId: 7 });
    expect(await nextJsonType(guest, "start_confirm")).toMatchObject({ startId: 7 });

    const lateJoin = await exports.default.fetch(new Request(`http://local.test/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`, {
      headers: { Upgrade: "websocket" },
    }));
    expect(lateJoin.status).toBe(409);
    expect(await lateJoin.json()).toMatchObject({ error: "late_join_unsupported" });

    const leftAtHost = nextMessage(host);
    guest.close(1000, "leaving");
    expect(JSON.parse(String((await leftAtHost).data))).toMatchObject({ type: "player_left", playerId: 1 });
    host.close(1000, "leaving");
  });

  it("rejects duplicates, invalid joins, and reconnect after start", async () => {
    const missing = await exports.default.fetch(new Request("http://local.test/v1/rooms/ABC234/connect?role=guest&build=test&gameplay=5", {
      headers: { Upgrade: "websocket" },
    }));
    expect(missing.status).toBe(404);
    expect(await missing.json()).toMatchObject({ error: "room_not_found" });

    const created = await exports.default.fetch(new Request("http://local.test/v1/rooms", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ gameplayVersion: 5 }),
    }));
    const room = await created.json() as { code: string; hostKey: string };
    const host = await connect(`/v1/rooms/${room.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(room.hostKey)}`);
    await nextJsonType(host, "welcome");
    await nextJsonType(host, "lobby_state");

    const duplicateHost = await exports.default.fetch(new Request(`http://local.test/v1/rooms/${room.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(room.hostKey)}`, {
      headers: { Upgrade: "websocket" },
    }));
    expect(await duplicateHost.json()).toMatchObject({ error: "host_already_connected" });

    const incompatible = await exports.default.fetch(new Request(`http://local.test/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=4`, {
      headers: { Upgrade: "websocket" },
    }));
    expect(await incompatible.json()).toMatchObject({ error: "incompatible_build" });

    const guest = await connect(`/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`);
    await nextJsonType(guest, "welcome");
    await nextJsonType(host, "player_joined");
    await nextJsonType(host, "lobby_state");
    await nextJsonType(guest, "lobby_state");
    const duplicateGuest = await exports.default.fetch(new Request(`http://local.test/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`, {
      headers: { Upgrade: "websocket" },
    }));
    expect(await duplicateGuest.json()).toMatchObject({ error: "room_full" });

    host.send(JSON.stringify({ type: "start_match", startId: 9, gameplayVersion: 5 }));
    await nextJsonType(host, "start_match");
    await nextJsonType(guest, "start_match");
    guest.close(1000, "leaving");
    await nextJsonType(host, "player_left");
    const reconnect = await exports.default.fetch(new Request(`http://local.test/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`, {
      headers: { Upgrade: "websocket" },
    }));
    expect(await reconnect.json()).toMatchObject({ error: "late_join_unsupported" });
    host.close(1000, "leaving");
  });

  it("invalidates the room when the host departs", async () => {
    const created = await exports.default.fetch(new Request("http://local.test/v1/rooms", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ gameplayVersion: 5 }),
    }));
    const room = await created.json() as { code: string; hostKey: string };
    const host = await connect(`/v1/rooms/${room.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(room.hostKey)}`);
    await nextJsonType(host, "welcome");
    await nextJsonType(host, "lobby_state");
    const guest = await connect(`/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`);
    await nextJsonType(guest, "welcome");
    await nextJsonType(host, "player_joined");
    await nextJsonType(host, "lobby_state");
    await nextJsonType(guest, "lobby_state");
    const matchClosed = nextJsonType(guest, "match_closed");
    host.close(1000, "leaving");
    expect(await matchClosed).toMatchObject({ reason: "host_left" });
    const stale = await exports.default.fetch(new Request(`http://local.test/v1/rooms/${room.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(room.hostKey)}`, {
      headers: { Upgrade: "websocket" },
    }));
    expect(stale.status).toBe(404);
    expect(await stale.json()).toMatchObject({ error: "room_not_found" });
  });

  it("expires a silent guest once while the host and room continue", async () => {
    const created = await exports.default.fetch(new Request("http://local.test/v1/rooms", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ gameplayVersion: 5 }),
    }));
    const room = await created.json() as { code: string; hostKey: string };
    const host = await connect(`/v1/rooms/${room.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(room.hostKey)}`);
    await nextJsonType(host, "welcome");
    await nextJsonType(host, "lobby_state");
    const guest = await connect(`/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`);
    await nextJsonType(guest, "welcome");
    await nextJsonType(host, "player_joined");
    await nextJsonType(host, "lobby_state");
    await nextJsonType(guest, "lobby_state");

    const now = Date.now();
    await setPeerActivity(room.code, now, now - PEER_SILENCE_TIMEOUT_MS - 1);
    await expirePeers(room.code, now);
    expect(await nextJsonType(host, "player_left")).toMatchObject({
      playerId: 1, code: 4008, reason: "guest_timeout",
    });
    expect(await nextJsonType(host, "lobby_state")).toMatchObject({ playerCount: 1 });
    await expirePeers(room.code, now + 1);
    host.send(JSON.stringify({ type: "heartbeat", sentAt: 9 }));
    expect(await nextJsonType(host, "heartbeat_ack")).toMatchObject({ sentAt: 9 });
    host.close(1000, "leaving");
  });

  it("expires a silent host, rejects stale room traffic, and permits a fresh room", async () => {
    const created = await exports.default.fetch(new Request("http://local.test/v1/rooms", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ gameplayVersion: 5 }),
    }));
    const room = await created.json() as { code: string; hostKey: string };
    const host = await connect(`/v1/rooms/${room.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(room.hostKey)}`);
    await nextJsonType(host, "welcome");
    await nextJsonType(host, "lobby_state");
    const guest = await connect(`/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`);
    await nextJsonType(guest, "welcome");
    await nextJsonType(host, "player_joined");
    await nextJsonType(host, "lobby_state");
    await nextJsonType(guest, "lobby_state");

    const now = Date.now();
    await setPeerActivity(room.code, now - PEER_SILENCE_TIMEOUT_MS - 1, now);
    const closed = nextJsonType(guest, "match_closed");
    await expirePeers(room.code, now);
    expect(await closed).toMatchObject({ reason: "host_timeout" });
    await expirePeers(room.code, now + 1);
    const stale = await exports.default.fetch(new Request(`http://local.test/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`, {
      headers: { Upgrade: "websocket" },
    }));
    expect(stale.status).toBe(404);
    expect(await stale.json()).toMatchObject({ error: "room_not_found" });

    const replacement = await exports.default.fetch(new Request("http://local.test/v1/rooms", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ gameplayVersion: 5 }),
    }));
    expect(replacement.status).toBe(201);
    const nextRoom = await replacement.json() as { code: string; hostKey: string };
    const nextHost = await connect(`/v1/rooms/${nextRoom.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(nextRoom.hostKey)}`);
    expect(await nextJsonType(nextHost, "welcome")).toMatchObject({ room: nextRoom.code });
    nextHost.close(1000, "leaving");
  });

  it("refreshes a peer deadline when activity arrives", async () => {
    const created = await exports.default.fetch(new Request("http://local.test/v1/rooms", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ gameplayVersion: 5 }),
    }));
    const room = await created.json() as { code: string; hostKey: string };
    const host = await connect(`/v1/rooms/${room.code}/connect?role=host&build=test&gameplay=5&key=${encodeURIComponent(room.hostKey)}`);
    await nextJsonType(host, "welcome");
    await nextJsonType(host, "lobby_state");
    const guest = await connect(`/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`);
    await nextJsonType(guest, "welcome");
    await nextJsonType(host, "player_joined");
    await nextJsonType(host, "lobby_state");
    await nextJsonType(guest, "lobby_state");

    const old = Date.now() - PEER_SILENCE_TIMEOUT_MS + 100;
    await setPeerActivity(room.code, old, old);
    guest.send(JSON.stringify({ type: "heartbeat", sentAt: 11 }));
    expect(await nextJsonType(guest, "heartbeat_ack")).toMatchObject({ sentAt: 11 });
    expect(await peerActivity(room.code, "guest")).toBeGreaterThan(old);
    const now = Date.now();
    const guestAt = await peerActivity(room.code, "guest");
    await setPeerActivity(room.code, now, guestAt);
    await expirePeers(room.code, now + PEER_SILENCE_TIMEOUT_MS - 1);
    host.send(JSON.stringify({ type: "heartbeat", sentAt: 12 }));
    expect(await nextJsonType(host, "heartbeat_ack")).toMatchObject({ sentAt: 12 });
    host.close(1000, "leaving");
  });
});
