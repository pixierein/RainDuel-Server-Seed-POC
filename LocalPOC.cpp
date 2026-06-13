#include <openssl/sha.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

vector<unsigned char> Sha512Bytes(const string& Text) {
    vector<unsigned char> Hash(SHA512_DIGEST_LENGTH);
    SHA512((const unsigned char*)Text.data(), Text.size(), Hash.data());
    return Hash;
}

string Sha512Hex(const string& Text) {
    vector<unsigned char> Hash = Sha512Bytes(Text);
    stringstream Out;

    for (int I = 0; I < (int)Hash.size(); I++) {
        Out << hex << setw(2) << setfill('0') << (int)Hash[I];
    }

    return Out.str();
}

struct HashStream {
    string ClientSeed;
    string ServerSeed;
    long long Nonce;
    int Round = 0;
    int Pos = 0;
    vector<unsigned char> Bytes;
};

int NextByte(HashStream& Stream) {
    if (Stream.Pos >= (int)Stream.Bytes.size()) {
        string Text = Stream.ClientSeed + ":" + Stream.ServerSeed + ":" + to_string(Stream.Nonce) + ":" + to_string(Stream.Round);
        Stream.Bytes = Sha512Bytes(Text);
        Stream.Pos = 0;
        Stream.Round++;
    }

    int Value = Stream.Bytes[Stream.Pos];
    Stream.Pos++;

    return Value;
}

int GetRandom(HashStream& Stream, int Max) {
    int Count = Max + 1;
    int Limit = (256 / Count) * Count;
    int Value = NextByte(Stream);

    while (Value >= Limit) {
        Value = NextByte(Stream);
    }

    return Value % Count;
}

vector<int> MakeMines(string ClientSeed, string ServerSeed, long long Nonce, int MineCount) {
    vector<int> Tiles;

    for (int I = 0; I < 25; I++) {
        Tiles.push_back(I);
    }

    HashStream Stream;
    Stream.ClientSeed = ClientSeed;
    Stream.ServerSeed = ServerSeed;
    Stream.Nonce = Nonce;

    for (int I = 24; I > 0; I--) {
        int Pick = GetRandom(Stream, I);
        swap(Tiles[I], Tiles[Pick]);
    }

    vector<int> Mines;

    for (int I = 0; I < MineCount; I++) {
        Mines.push_back(Tiles[I]);
    }

    sort(Mines.begin(), Mines.end());

    return Mines;
}

int main(int ArgCount, char* Args[]) {
    if (ArgCount != 6) {
        cout << "Use: " << Args[0] << " clientSeed serverSeed serverHash nonce mineCount\n";
        return 1;
    }

    string ClientSeed = Args[1];
    string ServerSeed = Args[2];
    string ServerHash = Args[3];
    long long Nonce = stoll(Args[4]);
    int MineCount = stoi(Args[5]);

    if (MineCount < 1 || MineCount > 24) {
        cout << "MineCount has to be from 1 to 24\n";
        return 1;
    }

    string CheckHash = Sha512Hex(ServerSeed);

    cout << "ClientSeed: " << ClientSeed << "\n";
    cout << "ServerSeed: " << ServerSeed << "\n";
    cout << "ServerHash: " << ServerHash << "\n";
    cout << "Nonce: " << Nonce << "\n";
    cout << "MineCount: " << MineCount << "\n";
    cout << "CheckHash: " << CheckHash << "\n";

    if (CheckHash != ServerHash) {
        cout << "Server seed hash does not match\n";
        return 1;
    }

    vector<int> Mines = MakeMines(ClientSeed, ServerSeed, Nonce, MineCount);
    vector<int> Safe;

    for (int I = 0; I < 25; I++) {
        if (find(Mines.begin(), Mines.end(), I) == Mines.end()) {
            Safe.push_back(I);
        }
    }

    cout << "Mines: ";
    for (int I = 0; I < (int)Mines.size(); I++) {
        cout << Mines[I] << " ";
    }

    cout << "\nSafe: ";
    for (int I = 0; I < (int)Safe.size(); I++) {
        cout << Safe[I] << " ";
    }

    cout << "\n";

    return 0;
}
