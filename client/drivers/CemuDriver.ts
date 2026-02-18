import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class CemuDriver extends BaseDriver {
  family = "cemu";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
  }
}
