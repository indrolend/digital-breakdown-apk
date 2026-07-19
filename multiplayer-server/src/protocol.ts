export const PROTOCOL_VERSION = 3;
export const MAX_PLAYERS = 4;
export const MAX_MESSAGE_BYTES = 64 * 1024;
export const ROOM_CODE_LENGTH = 6;
export const ROOM_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
export const BINARY_HEADER_BYTES = 20;
export const BINARY_MAGIC = 0x504d4244;

export type ClientMessageType = "input" | "snapshot" | "event" | "ping" | "pong";

export interface WireEnvelope {
  v: number;
  type: ClientMessageType;
  seq: number;
  tick: number;
  payload?: unknown;
}

export interface BinaryHeader {
  type: number;
  playerId: number;
  sequence: number;
  tick: number;
  payloadBytes: number;
}

export function makeRoomCode(bytes: Uint8Array): string {
  if (bytes.length < ROOM_CODE_LENGTH) throw new Error("room code needs six random bytes");
  let code = "";
  for (let i = 0; i < ROOM_CODE_LENGTH; ++i) {
    code += ROOM_ALPHABET[bytes[i]! % ROOM_ALPHABET.length]!;
  }
  return code;
}

export function isRoomCode(value: string): boolean {
  return value.length === ROOM_CODE_LENGTH && [...value].every((c) => ROOM_ALPHABET.includes(c));
}

export function parseEnvelope(message: string): WireEnvelope | null {
  if (new TextEncoder().encode(message).byteLength > MAX_MESSAGE_BYTES) return null;
  let value: unknown;
  try { value = JSON.parse(message); } catch { return null; }
  if (!value || typeof value !== "object") return null;
  const record = value as Record<string, unknown>;
  if (record.v !== PROTOCOL_VERSION) return null;
  if (!(["input", "snapshot", "event", "ping", "pong"] as const).includes(record.type as ClientMessageType)) return null;
  if (!Number.isSafeInteger(record.seq) || (record.seq as number) < 0) return null;
  if (!Number.isSafeInteger(record.tick) || (record.tick as number) < 0) return null;
  return { v: PROTOCOL_VERSION, type: record.type as ClientMessageType, seq: record.seq as number, tick: record.tick as number, payload: record.payload };
}

export function parseBinaryHeader(message: ArrayBuffer): BinaryHeader | null {
  if (message.byteLength < BINARY_HEADER_BYTES || message.byteLength > MAX_MESSAGE_BYTES) return null;
  const view = new DataView(message);
  if (view.getUint32(0, true) !== BINARY_MAGIC || view.getUint16(4, true) !== PROTOCOL_VERSION) return null;
  const type = view.getUint8(6);
  const playerId = view.getUint8(7);
  const sequence = view.getUint32(8, true);
  const tick = view.getUint32(12, true);
  const payloadBytes = view.getUint32(16, true);
  if (type < 1 || type > 5 || playerId >= MAX_PLAYERS || payloadBytes !== message.byteLength - BINARY_HEADER_BYTES) return null;
  return { type, playerId, sequence, tick, payloadBytes };
}

export function jsonResponse(value: unknown, status = 200): Response {
  return Response.json(value, { status, headers: { "Cache-Control": "no-store" } });
}
