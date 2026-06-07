# RainDuel Mines Predictor

## I'll be honest, the developers here weren't too competent about how they made their website; that explains why they exit scammed.

## What It Does

This predicts mine locations in RainDuel's Mines game by exploiting the provably fair system's seed rotation mechanism.

## How It Works

1. **Fetch current seeds** — Gets the active `clientSeed`, `serverSeedHash`, and `nonce` from `/api/seeds`

2. **Start a game** — Begins a mines round with `betCents: 0` (free play) to lock in the current server seed

3. **Rotate seeds** — Calls `/api/seeds/rotate` which reveals the old `serverSeed` in plaintext. This is the critical step — the server hands over the seed that was just used

4. **Verify integrity** — SHA-512 hashes the revealed seed and confirms it matches the hash the game was started with

5. **Compute mines locally** — Replicates the server's shuffle algorithm:
   - Creates a deterministic byte stream from `SHA-512(clientSeed:serverSeed:nonce:round)`
   - Uses rejection sampling to avoid modulo bias
   - Fisher-Yates shuffles 25 positions, first N are mines

6. **Reveal safe tiles only** — Steps through every non-mine position via `/api/mines/reveal`

7. **Cash out** — Collects the payout with a near-perfect multiplier

## Why It Exists

RainDuel's provably fair system is designed to let players verify game outcomes after the fact. The flaw is that seed rotation happens *during* an active game — the server reveals the current game's seed before the game ends. This creates a window where the player knows the exact mine layout before cashing out.

The intended design should be: reveal seed **after** game ends, not before.

## Limitations

- Requires authenticated session cookies (login via curl first)
- `betCents: 0` only works if the site allows free play
- Server could patch this by delaying seed reveal until after cashout/loss
