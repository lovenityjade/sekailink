import { BaseDriver } from "./BaseDriver";
import { DriverContext } from "../runtime/types";

export class DuckStationDriver extends BaseDriver {
  family = "duckstation";

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
  }
}
