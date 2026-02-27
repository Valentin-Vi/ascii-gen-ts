import type { Cell, ConvertOptions } from './converter';

// eslint-disable-next-line @typescript-eslint/no-require-imports
const addon = require('../build/Release/iag_native') as {
  convertFrameNative(
    pixels: Buffer,
    fw: number,
    fh: number,
    cols: number,
    rows: number,
    ramp: string,
  ): Array<{ char: string; r: number; g: number; b: number }>;
};

const DEFAULT_RAMP = ' .,:;+*?%S#@';
const DEFAULT_COLS = 80;
const DEFAULT_ROWS = 40;

export function convertFrameNative(
  pixels: Uint8Array,
  frameWidth: number,
  frameHeight: number,
  options?: ConvertOptions,
): Cell[][] {
  const cols = options?.cols ?? DEFAULT_COLS;
  const rows = options?.rows ?? DEFAULT_ROWS;
  const ramp = options?.ramp ?? DEFAULT_RAMP;

  const buf = Buffer.isBuffer(pixels)
    ? pixels
    : Buffer.from(pixels.buffer, pixels.byteOffset, pixels.byteLength);

  const flat = addon.convertFrameNative(buf, frameWidth, frameHeight, cols, rows, ramp);

  const result: Cell[][] = [];
  for (let row = 0; row < rows; row++) {
    const rowCells: Cell[] = [];
    for (let col = 0; col < cols; col++) {
      rowCells.push(flat[row * cols + col]);
    }
    result.push(rowCells);
  }

  return result;
}
