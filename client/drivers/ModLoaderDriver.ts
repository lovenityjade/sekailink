import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class ModLoaderDriver extends BaseDriver {
  family = "mod_loader";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: install mod loader + mods, write config
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
  }
}
