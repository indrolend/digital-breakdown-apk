import { describe, expect, it } from "vitest";
import { BINARY_HEADER_BYTES, BINARY_MAGIC, MAX_MESSAGE_BYTES, PROTOCOL_VERSION, isRoomCode, makeRoomCode, parseBinaryHeader, parseEnvelope } from "../src/protocol";

describe("room codes", () => {
  it("uses six unambiguous uppercase characters", () => {
    expect(makeRoomCode(new Uint8Array([0, 1, 2, 3, 4, 5]))).toBe("ABCDEF");
    expect(isRoomCode("ABCDEF")).toBe(true);
    expect(isRoomCode("ABC0EF")).toBe(false);
    expect(isRoomCode("abcDEF")).toBe(false);
  });
});

describe("binary gameplay packets", () => {
  it("reads the explicit little-endian header", () => {
    const packet = new ArrayBuffer(BINARY_HEADER_BYTES + 3);
    const view = new DataView(packet);
    view.setUint32(0, BINARY_MAGIC, true); view.setUint16(4, PROTOCOL_VERSION, true); view.setUint8(6, 1); view.setUint8(7, 2);
    view.setUint32(8, 19, true); view.setUint32(12, 44, true); view.setUint32(16, 3, true);
    expect(parseBinaryHeader(packet)).toEqual({ type: 1, playerId: 2, sequence: 19, tick: 44, payloadBytes: 3 });
  });

  it("rejects truncated and inconsistent payloads", () => {
    expect(parseBinaryHeader(new ArrayBuffer(4))).toBeNull();
    const packet = new ArrayBuffer(BINARY_HEADER_BYTES);
    const view = new DataView(packet); view.setUint32(0, BINARY_MAGIC, true); view.setUint16(4, PROTOCOL_VERSION, true); view.setUint8(6, 1); view.setUint8(7, 0); view.setUint32(16, 99, true);
    expect(parseBinaryHeader(packet)).toBeNull();
  });
});

describe("wire envelopes", () => {
  it("accepts the current protocol", () => {
    expect(parseEnvelope(JSON.stringify({ v: PROTOCOL_VERSION, type: "input", seq: 4, tick: 12, payload: { moveX: 1 } })))
      .toMatchObject({ v: PROTOCOL_VERSION, type: "input", seq: 4, tick: 12 });
  });

  it("rejects incompatible, malformed, and oversized messages", () => {
    expect(parseEnvelope("{")) .toBeNull();
    expect(parseEnvelope(JSON.stringify({ v: PROTOCOL_VERSION - 1, type: "input", seq: 1, tick: 1 }))).toBeNull();
    expect(parseEnvelope(JSON.stringify({ v: PROTOCOL_VERSION, type: "unknown", seq: 1, tick: 1 }))).toBeNull();
    expect(parseEnvelope("x".repeat(MAX_MESSAGE_BYTES + 1))).toBeNull();
  });
});
