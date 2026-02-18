import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class RetroArchDriver extends BaseDriver {
  family = "retroarch";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: write retroarch.cfg + core configs, install cores if missing
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: launch RetroArch with core and ROM
  }

  async connect(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: start client + auto-connect
  }
}
