import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class SwitchDriver extends BaseDriver {
  family = "switch";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: configure keys/firmware paths (user-provided)
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
  }
}
