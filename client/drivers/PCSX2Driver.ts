import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class PCSX2Driver extends BaseDriver {
  family = "pcsx2";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
  }
}
