export interface Cell {
  char: string;
  r: number;
  g: number;
  b: number;
}

export interface ConvertOptions {
  cols?: number;
  rows?: number;
  ramp?: string;
}

const DEFAULT_RAMP = ' .,:;+*?%S#@';
const DEFAULT_COLS = 80;
const DEFAULT_ROWS = 40;

export function convertFrame(
  pixels: Uint8Array,
  frameWidth: number,
  frameHeight: number,
  options?: ConvertOptions,
): Cell[][] {
  const cols = options?.cols ?? DEFAULT_COLS;
  const rows = options?.rows ?? DEFAULT_ROWS;
  const ramp = options?.ramp ?? DEFAULT_RAMP;

  if (ramp.length < 2 || ramp.length > 1024) {
    throw new RangeError(`ramp length must be between 2 and 1024, got ${ramp.length}`);
  }
  if (cols <= 0 || rows <= 0) {
    throw new RangeError(`cols and rows must be positive, got cols=${cols} rows=${rows}`);
  }
  if (frameWidth <= 0 || frameHeight <= 0) {
    throw new RangeError(
      `frameWidth and frameHeight must be positive, got ${frameWidth}x${frameHeight}`,
    );
  }
  if (pixels.length < frameWidth * frameHeight * 4) {
    throw new RangeError(
      `pixels buffer too small: expected at least ${frameWidth * frameHeight * 4} bytes`,
    );
  }

  const rampLen = ramp.length;
  const result: Cell[][] = [];

  for (let row = 0; row < rows; row++) {
    const rowCells: Cell[] = [];

    for (let col = 0; col < cols; col++) {
      const xStart = (col * frameWidth) / cols;
      const xEnd = ((col + 1) * frameWidth) / cols;
      const yStart = (row * frameHeight) / rows;
      const yEnd = ((row + 1) * frameHeight) / rows;

      const pxStart = Math.floor(xStart);
      const pxEnd = Math.ceil(xEnd);
      const pyStart = Math.floor(yStart);
      const pyEnd = Math.ceil(yEnd);

      let accR = 0, accG = 0, accB = 0, accW = 0;

      for (let py = pyStart; py < pyEnd; py++) {
        const wy = Math.min(py + 1, yEnd) - Math.max(py, yStart);
        if (wy <= 0) continue;

        for (let px = pxStart; px < pxEnd; px++) {
          const wx = Math.min(px + 1, xEnd) - Math.max(px, xStart);
          if (wx <= 0) continue;

          const w = wx * wy;
          const offset = (py * frameWidth + px) * 4;

          accR += pixels[offset] * w;
          accG += pixels[offset + 1] * w;
          accB += pixels[offset + 2] * w;
          accW += w;
        }
      }

      let r = 0, g = 0, b = 0;
      if (accW > 0) {
        r = Math.trunc(accR / accW);
        g = Math.trunc(accG / accW);
        b = Math.trunc(accB / accW);
      }

      // BT.601 luma
      const luma = 0.299 * r + 0.587 * g + 0.114 * b;
      let idx = Math.trunc((luma * (rampLen - 1)) / 255);
      if (idx >= rampLen) idx = rampLen - 1;

      rowCells.push({ char: ramp[idx], r, g, b });
    }

    result.push(rowCells);
  }

  return result;
}
