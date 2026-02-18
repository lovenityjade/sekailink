import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class DolphinDriver extends BaseDriver {
  family = "dolphin";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: write Dolphin config + place save/REL assets
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: launch Dolphin with game ISO
  }
}
