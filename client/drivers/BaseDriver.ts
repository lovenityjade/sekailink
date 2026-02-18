import { DriverContext } from "../runtime/types";

export abstract class BaseDriver {
  abstract family: string;

  async verify(ctx: DriverContext): Promise<void> {
    void ctx;
  }

  async prepare(ctx: DriverContext): Promise<void> {
    void ctx;
  }

  async launch(ctx: DriverContext): Promise<void> {
    void ctx;
  }

  async connect(ctx: DriverContext): Promise<void> {
    void ctx;
  }
}
