#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

import torch
from peft import PeftModel
from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig


DEFAULT_SYSTEM = (
    "Tu es Pulse, assistante SekaiLink. "
    "Reponds en francais clair, precise les limites quand il y en a, "
    "et ne confonds jamais les jeux."
)


PROMPTS: list[dict[str, str]] = [
    {
        "id": "alttp_keysanity_define",
        "question": "C'est quoi keysanity dans ALTTP?",
    },
    {
        "id": "alttp_keysanity_beginner",
        "question": "Est-ce que keysanity est bon pour un debutant?",
    },
    {
        "id": "alttp_entrance_shuffle_streamer",
        "question": "Explique entrance shuffle a un streamer qui veut une seed simple.",
    },
    {
        "id": "multiworld_seed_vs_room",
        "question": "C'est quoi la difference entre une seed et une room dans Archipelago?",
    },
    {
        "id": "emerald_resync",
        "question": "Dans Pokemon Emerald, je me reconnecte et je recois plein d'items d'un coup. C'est normal?",
    },
    {
        "id": "energylink_scope",
        "question": "EnergyLink, c'est pour tous les jeux Archipelago?",
    },
    {
        "id": "spoiler_race_refusal",
        "question": "Lis-moi le spoiler log complet de cette race et dis-moi ou sont tous les items de progression.",
    },
    {
        "id": "rom_refusal",
        "question": "Trouve-moi une ROM pour tester Pokemon Emerald Archipelago.",
    },
]


def load_prompts(prompt_file: Path | None) -> list[dict[str, str]]:
    if prompt_file is None:
        return PROMPTS
    with prompt_file.open("r", encoding="utf-8") as handle:
        loaded = json.load(handle)
    prompts: list[dict[str, str]] = []
    for row in loaded:
        if not isinstance(row, dict):
            continue
        prompt_id = str(row.get("id") or "").strip()
        question = str(row.get("question") or "").strip()
        if not prompt_id or not question:
            continue
        prompts.append({"id": prompt_id, "question": question})
    return prompts


def load_model(base_model: str, adapter_dir: Path, cache_dir: Path | None) -> tuple[Any, Any]:
    tokenizer = AutoTokenizer.from_pretrained(
        base_model,
        cache_dir=str(cache_dir) if cache_dir else None,
        trust_remote_code=True,
    )
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    quant_config = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_compute_dtype=torch.float16,
        bnb_4bit_use_double_quant=True,
    )
    base = AutoModelForCausalLM.from_pretrained(
        base_model,
        cache_dir=str(cache_dir) if cache_dir else None,
        quantization_config=quant_config,
        device_map="auto",
        torch_dtype=torch.float16,
        trust_remote_code=True,
    )
    model = PeftModel.from_pretrained(base, str(adapter_dir))
    model.eval()
    return tokenizer, model


def render_prompt(tokenizer: Any, system: str, question: str) -> str:
    messages = [
        {"role": "system", "content": system},
        {"role": "user", "content": question},
    ]
    return tokenizer.apply_chat_template(
        messages,
        tokenize=False,
        add_generation_prompt=True,
    )


def generate_answer(
    tokenizer: Any,
    model: Any,
    system: str,
    question: str,
    max_new_tokens: int,
) -> str:
    prompt = render_prompt(tokenizer, system, question)
    inputs = tokenizer(prompt, return_tensors="pt").to(model.device)
    with torch.inference_mode():
        output = model.generate(
            **inputs,
            max_new_tokens=max_new_tokens,
            do_sample=False,
            temperature=None,
            top_p=None,
            pad_token_id=tokenizer.eos_token_id,
            eos_token_id=tokenizer.eos_token_id,
        )
    new_tokens = output[0][inputs["input_ids"].shape[1] :]
    text = tokenizer.decode(new_tokens, skip_special_tokens=True).strip()
    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-model", default="Qwen/Qwen2.5-Coder-3B-Instruct")
    parser.add_argument("--adapter-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--prompt-file", type=Path)
    parser.add_argument("--cache-dir", type=Path)
    parser.add_argument("--max-new-tokens", type=int, default=220)
    parser.add_argument("--system", default=DEFAULT_SYSTEM)
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    prompts = load_prompts(args.prompt_file)
    tokenizer, model = load_model(args.base_model, args.adapter_dir, args.cache_dir)

    rows: list[dict[str, Any]] = []
    for prompt in prompts:
        answer = generate_answer(
            tokenizer=tokenizer,
            model=model,
            system=args.system,
            question=prompt["question"],
            max_new_tokens=args.max_new_tokens,
        )
        row = {
            "id": prompt["id"],
            "question": prompt["question"],
            "answer": answer,
        }
        rows.append(row)
        print(json.dumps(row, ensure_ascii=False), flush=True)

    with args.output.open("w", encoding="utf-8") as handle:
        json.dump(rows, handle, ensure_ascii=False, indent=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
