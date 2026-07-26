import { exports } from "cloudflare:workers";
import { describe, expect, it } from "vitest";
import { BINARY_HEADER_BYTES, BINARY_MAGIC, PROTOCOL_VERSION } from "../src/protocol";

function nextMessage(socket: WebSocket, timeoutMs = 2000): Promise<MessageEvent> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("websocket message timeout")), timeoutMs);
    socket.addEventListener("message", (event) => {
      clearTimeout(timer);
      resolve(event);
    }, { once: true });
  });
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
  return socket;
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
    const hostWelcome = JSON.parse(String((await nextMessage(host)).data));
    expect(hostWelcome).toMatchObject({ type: "welcome", playerId: 0, role: "host", room: room.code });

    const joinedAtHost = nextMessage(host);
    const guest = await connect(`/v1/rooms/${room.code}/connect?role=guest&build=test&gameplay=5`);
    const guestWelcome = JSON.parse(String((await nextMessage(guest)).data));
    expect(guestWelcome).toMatchObject({ type: "welcome", playerId: 1, role: "guest", room: room.code });
    expect(JSON.parse(String((await joinedAtHost).data))).toMatchObject({ type: "player_joined", playerId: 1 });

    const guestInput = packet(1, 1, 1);
    const inputAtHost = nextMessage(host);
    guest.send(guestInput);
    expect((await inputAtHost).data).toBeInstanceOf(ArrayBuffer);

    const hostSnapshot = packet(2, 0, 2);
    const snapshotAtGuest = nextMessage(guest);
    host.send(hostSnapshot);
    expect((await snapshotAtGuest).data).toBeInstanceOf(ArrayBuffer);

    const leftAtHost = nextMessage(host);
    guest.close(1000, "leaving");
    expect(JSON.parse(String((await leftAtHost).data))).toMatchObject({ type: "player_left", playerId: 1 });
    host.close(1000, "leaving");
  });
});
