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
export declare function convertFrame(pixels: Uint8Array, frameWidth: number, frameHeight: number, options?: ConvertOptions): Cell[][];
