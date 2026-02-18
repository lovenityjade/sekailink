import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class BizHawkDriver extends BaseDriver {
  family = "bizhawk";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: write EmuHawk config + Lua core settings, place connector scripts
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: launch EmuHawk with ROM/ISO + lua connector args
  }

  async connect(ctx: DriverContext): Promise<void> {
    void ctx;
    // TODO: launch AP client and auto-connect
  }
}
