"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.renderToAnsi = renderToAnsi;
function renderToAnsi(cells, color = true) {
    const parts = [];
    for (const row of cells) {
        for (const cell of row) {
            if (color) {
                parts.push(`\x1b[38;2;${cell.r};${cell.g};${cell.b}m${cell.char}`);
            }
            else {
                parts.push(cell.char);
            }
        }
        if (color)
            parts.push('\x1b[0m');
        parts.push('\n');
    }
    return parts.join('');
}
