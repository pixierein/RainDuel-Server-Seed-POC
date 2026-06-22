#include <openssl/sha.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;


// converts input text into SHA-512 bytes
// used for generating deterministic random data
vector<unsigned char> Sha512Bytes(const string& Text) {

    vector<unsigned char> Hash(SHA512_DIGEST_LENGTH);

    SHA512(
        (const unsigned char*)Text.data(),
        Text.size(),
        Hash.data()
    );

    return Hash;
}


// converts SHA-512 output into readable hexadecimal format
string Sha512Hex(const string& Text) {

    vector<unsigned char> Hash = Sha512Bytes(Text);

    stringstream Out;


    for (int I = 0; I < (int)Hash.size(); I++) {

        // convert each byte into 2 digit hex
        Out << hex
            << setw(2)
            << setfill('0')
            << (int)Hash[I];
    }


    return Out.str();
}


// stores the current state of the deterministic random generator
// keeps track of seeds, nonce, and current position in hash data
struct HashStream {

    string ClientSeed;
    string ServerSeed;

    long long Nonce;

    int Round = 0;
    int Pos = 0;

    vector<unsigned char> Bytes;
};


// this gets the next random byte from the hash stream
// when the current hash runs out
// a new SHA512 hash is generated using: clientSeed:serverSeed:nonce:round
int NextByte(HashStream& Stream) {


    if (Stream.Pos >= (int)Stream.Bytes.size()) {


        string Text =
            Stream.ClientSeed + ":" +
            Stream.ServerSeed + ":" +
            to_string(Stream.Nonce) + ":" +
            to_string(Stream.Round);


        Stream.Bytes = Sha512Bytes(Text);

        Stream.Pos = 0;
        Stream.Round++;
    }


    int Value = Stream.Bytes[Stream.Pos];

    Stream.Pos++;

    return Value;
}


// generates a random number between 0 and infinity
// uses rejection sampling to avoid uneven random results
int GetRandom(HashStream& Stream, int Max) {


    int Count = Max + 1;


    // Prevents modulo bias
    int Limit = (256 / Count) * Count;


    int Value = NextByte(Stream);


    while (Value >= Limit) {

        Value = NextByte(Stream);

    }


    return Value % Count;
}


// generates mine positions using a cool shuffle
vector<int> MakeMines(
    string ClientSeed,
    string ServerSeed,
    long long Nonce,
    int MineCount
) {


    // create board tiles 0-24
    vector<int> Tiles;


    for (int I = 0; I < 25; I++) {

        Tiles.push_back(I);

    }


    HashStream Stream;

    Stream.ClientSeed = ClientSeed;
    Stream.ServerSeed = ServerSeed;
    Stream.Nonce = Nonce;



    // the good old fisher yates shuffle
    // uses generated random values instead of normal RNG
    for (int I = 24; I > 0; I--) {

        int Pick = GetRandom(Stream, I);

        swap(Tiles[I], Tiles[Pick]);

    }



    // first minecount tiles become mines
    vector<int> Mines;


    for (int I = 0; I < MineCount; I++) {

        Mines.push_back(Tiles[I]);

    }


    sort(Mines.begin(), Mines.end());


    return Mines;
}


int main(int ArgCount, char* Args[]) {


    // clientseed serverseed serverhash nonce minecount
    if (ArgCount != 6) {

        cout << "Use: "
             << Args[0]
             << " clientSeed serverSeed serverHash nonce mineCount\n";

        return 1;
    }



    string ClientSeed = Args[1];
    string ServerSeed = Args[2];
    string ServerHash = Args[3];

    long long Nonce = stoll(Args[4]);

    int MineCount = stoi(Args[5]);



    // validate mine amount
    if (MineCount < 1 || MineCount > 24) {

        cout << "MineCount has to be from 1 to 24\n";

        return 1;
    }



    // verify server seed matches provided hash
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



    // gen mine positions
    vector<int> Mines =
        MakeMines(ClientSeed, ServerSeed, Nonce, MineCount);



    // find remaining safe tiles
    vector<int> Safe;


    for (int I = 0; I < 25; I++) {

        if (find(Mines.begin(), Mines.end(), I) == Mines.end()) {

            Safe.push_back(I);

        }
    }



    cout << "Mines: ";

    for (int I : Mines)

        cout << I << " ";



    cout << "\nSafe: ";

    for (int I : Safe)

        cout << I << " ";


    cout << "\n";


    return 0;
}
