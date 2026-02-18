import React from "react";
import { expect, it, vi, beforeEach } from "vitest";
import { render, screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter } from "react-router-dom";
import RoomListPage from "../pages/RoomList";
import GameManagerPage from "../pages/GameManager";
import AccountPage from "../pages/Account";

vi.mock("socket.io-client", () => {
  return {
    io: () => ({
      on: vi.fn(),
      emit: vi.fn(),
      disconnect: vi.fn()
    })
  };
});

const jsonResponse = (data: unknown, status = 200) =>
  new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json" }
  });

beforeEach(() => {
  vi.resetAllMocks();
});

it("loads lobbies and creates a room", async () => {
  const fetchMock = vi.mocked(globalThis.fetch as unknown as typeof fetch);
  fetchMock
    .mockResolvedValueOnce(jsonResponse({ lobbies: [{ id: "abc", name: "Test Lobby", owner: "Owner" }] }))
    .mockResolvedValueOnce(jsonResponse({ url: "/lobby/abc" }, 201));

  render(
    <MemoryRouter>
      <RoomListPage />
    </MemoryRouter>
  );

  await waitFor(() => expect(screen.getByText("Test Lobby")).toBeInTheDocument());

  const createButtons = screen.getAllByRole("button", { name: /create room/i });
  await userEvent.click(createButtons[0]);
  const nameInput = screen.getByPlaceholderText("Room name");
  const descInput = screen.getByPlaceholderText("Short description");

  await userEvent.type(nameInput, "My Room");
  await userEvent.type(descInput, "Hello");

  const modal = screen.getByRole("dialog", { name: /create room/i });
  const submitButton = within(modal).getByRole("button", { name: /create room/i });
  await userEvent.click(submitButton);

  await waitFor(() => {
    const postCall = fetchMock.mock.calls.find(
      (call) => String(call[0]).includes("/api/lobbies") && call[1] && (call[1] as RequestInit).method === "POST"
    );
    expect(postCall).toBeTruthy();
  });
});

it("loads yamls and preselects yaml for lobby", async () => {
  const fetchMock = vi.mocked(globalThis.fetch as unknown as typeof fetch);
  fetchMock.mockResolvedValueOnce(jsonResponse({ yamls: [{ id: "y1", title: "Seed", game: "Game", player_name: "P" }] }));

  render(
    <MemoryRouter>
      <GameManagerPage />
    </MemoryRouter>
  );

  await waitFor(() => expect(screen.getByText("Seed")).toBeInTheDocument());

  const selectButton = screen.getByRole("button", { name: "Select" });
  await userEvent.click(selectButton);

  await waitFor(() => {
    expect(window.localStorage.getItem("skl.activeYamlId")).toBe("y1");
  });
});

it("downloads a yaml from account", async () => {
  const fetchMock = vi.mocked(globalThis.fetch as unknown as typeof fetch);
  fetchMock
    .mockResolvedValueOnce(jsonResponse({ yamls: [{ id: "y1", title: "Seed", game: "Game", player_name: "P" }] }))
    .mockResolvedValueOnce(jsonResponse({ presence_status: "online", dm_policy: "anyone" }))
    .mockResolvedValueOnce(jsonResponse({ discord_id: "1", username: "User" }))
    .mockResolvedValueOnce(jsonResponse({ yaml: "game: Game\nname: P\n" }));

  render(
    <MemoryRouter>
      <AccountPage />
    </MemoryRouter>
  );

  await waitFor(() => expect(screen.getByText("Seed")).toBeInTheDocument());
  const downloadButton = screen.getByRole("button", { name: "Download" });
  await userEvent.click(downloadButton);

  await waitFor(() => {
    const match = fetchMock.mock.calls.find((call) =>
      String(call[0]).includes("/api/yamls/y1/raw")
    );
    expect(match).toBeTruthy();
  });
});
