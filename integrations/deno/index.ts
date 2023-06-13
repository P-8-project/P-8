import init, { compiler as viablec } from "./wasm/viable_wasm.js";

const initialized = init(Deno.readFile("./wasm/viable_wasm_bg.wasm"));

export async function compiler(source: string): Promise<string> {
  await initialized;
  return viablec(source);
}
