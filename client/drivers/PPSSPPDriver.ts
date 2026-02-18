import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class PPSSPPDriver extends BaseDriver {
  family = "ppsspp";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
  }
}
