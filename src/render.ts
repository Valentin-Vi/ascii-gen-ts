import type { Cell } from './converter';

export function renderToAnsi(cells: Cell[][], color = true): string {
  const parts: string[] = [];

  for (const row of cells) {
    for (const cell of row) {
      if (color) {
        parts.push(`\x1b[38;2;${cell.r};${cell.g};${cell.b}m${cell.char}`);
      } else {
        parts.push(cell.char);
      }
    }
    if (color) parts.push('\x1b[0m');
    parts.push('\n');
  }

  return parts.join('');
}
