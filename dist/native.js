"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.convertFrameNative = convertFrameNative;
let _addon;
function getAddon() {
    if (_addon)
        return _addon;
    try {
        // eslint-disable-next-line @typescript-eslint/no-require-imports
        _addon = require('../build/Release/iag_native');
        return _addon;
    }
    catch {
        throw new Error('ascii-gen-ts: native addon not built. ' +
            'Run `node-gyp rebuild` inside the package directory ' +
            'or use `convertFrame` (pure TypeScript) instead.');
    }
}
const DEFAULT_RAMP = ' .,:;+*?%S#@';
const DEFAULT_COLS = 80;
const DEFAULT_ROWS = 40;
function convertFrameNative(pixels, frameWidth, frameHeight, options) {
    const cols = options?.cols ?? DEFAULT_COLS;
    const rows = options?.rows ?? DEFAULT_ROWS;
    const ramp = options?.ramp ?? DEFAULT_RAMP;
    const buf = Buffer.isBuffer(pixels)
        ? pixels
        : Buffer.from(pixels.buffer, pixels.byteOffset, pixels.byteLength);
    const flat = getAddon().convertFrameNative(buf, frameWidth, frameHeight, cols, rows, ramp);
    const result = [];
    for (let row = 0; row < rows; row++) {
        const rowCells = [];
        for (let col = 0; col < cols; col++) {
            rowCells.push(flat[row * cols + col]);
        }
        result.push(rowCells);
    }
    return result;
}
