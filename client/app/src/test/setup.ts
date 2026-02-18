import "@testing-library/jest-dom";
import { vi } from "vitest";

globalThis.fetch = vi.fn();

if (!globalThis.URL.createObjectURL) {
  globalThis.URL.createObjectURL = vi.fn(() => "blob:mock");
}

if (!globalThis.URL.revokeObjectURL) {
  globalThis.URL.revokeObjectURL = vi.fn();
}

if (!globalThis.scrollTo) {
  globalThis.scrollTo = () => {};
}

HTMLAnchorElement.prototype.click = () => {};
