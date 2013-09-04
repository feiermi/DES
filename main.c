#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "tables.h"


#define DES_RELEASE           //debug used

typedef char ElemType;
/********INPUT AND OUTPUT**********/

char BitToHex(ElemType* bit)
{
    int i,temp=0;
    int ch;
    for(i=0; i!=4; ++i)
    {
        temp<<=1;
        temp+=(bit[i]&0x01);
    }
    if(temp>9)
        ch = temp-10 + 'A';
    else
        ch = temp + '0';
    return ch;
}

//print HEX to screen
void printHex(ElemType* bitstream, int BYTES)
{
    int cnt;
    for (cnt=0; cnt!=BYTES; ++cnt)
        printf("%c",BitToHex(bitstream+cnt*4));
    printf("\n");
}


void HexToStr(char* input, char* output)
{
    int temp,i;
    while(*input != '\0')
    {
        if(*input<='9' && *input >= '0')
            temp=*input-'0';
        else
            temp = *input - 'A'+10;
        for(i=0; i!=4; ++i)
            *(output++) = temp >> (3-i) & 1;
        ++input;
    }
}

/***********SUBKEY**********/

void Subkey_PC1(ElemType* key, ElemType* Out)
{
    //key is char[64], outL and outR are char[28]
    int i;
    for(i = 0; i < 28; ++i)
        Out[i]=key[PC1L_table[i]-1];
    for(i = 0; i < 28; ++i)
        Out[i+28]=key[PC1R_table[i]-1];
}


void Subkey_PC2(ElemType* input, ElemType* subkey)
{
    //input: L and R, char[28]; output: subkey, char[48];
    int i;
    for(i=0; i < 48; ++i)
        *(subkey+i)=input[PC2_table[i]-1];
}


void Subkey_LeftMove(ElemType* In, int mvcnt)
//mvcnt stands for move times,range 1 or 2
{
    ElemType temp;
    int i;
    while(mvcnt--)
    {
        //left move once
        temp=In[0];
        for(i=0; i!= 27; ++i)
            In[i]=In[i+1];
        In[27]=temp;
    }
}

void MakeSubKey(ElemType* key, ElemType* SubkeyArray)
{

    //input key of char[64] and output subkeyarray[16*48]
    int i;
    ElemType PC1out[56];
    Subkey_PC1(key,PC1out); // parse key into Left and right parts
    for(i=0; i!=16; ++i)
    {
        Subkey_LeftMove(PC1out,MOVE_table[i]);
        Subkey_LeftMove(PC1out+28,MOVE_table[i]);
        Subkey_PC2(PC1out,SubkeyArray+48*i);
    }
}

/************DES BODY*************/


ElemType* XOR(ElemType* In_1, ElemType* In_2, size_t bitnum) // 'In_1', 'In_2': ElemType [48], 'Out': ElemType [48]
{
    // XOR, In_1 = Expand function's Output, In_2 = SubKey, Out = Wait to be S_Box
    int i;
    for(i = 0; i < bitnum; ++i) // 'i' is the number of bit that we have XOR.
        In_1[i]=(In_1[i]^In_2[i])&0x01;

    return In_1;
}


void Substitution(ElemType* In, ElemType* Out) // 'In': ElemType [48], 'Out': ElemType [32]
{
    // The S_Box, In = XOR's Output, Out = Wait to be P function.
    int x,y,z;
    int InWordLeftBound,OutWordLeftBound;
    for(x = 0; x < 8; ++x)
    {
        InWordLeftBound = 6 * x;    //select the input word, since we have a 48bit input and 8 boxes,
        // the beginning index of each word should be 6*x
        OutWordLeftBound = x << 2;  //The same to ouputstream. But now we have only 4bit a word.

        //Translate the two ends into one index.
        y = (*(In + InWordLeftBound) << 1) | (*(In + InWordLeftBound + 5));

        //translate the middle part into one index.
        z = ((*(In + InWordLeftBound + 1) << 3) | (*(In + InWordLeftBound + 2) << 2) | (*(In + InWordLeftBound + 3) << 1) | (*(In + InWordLeftBound + 4)));


        //Output, translate the integer into char array.
        *(Out + OutWordLeftBound) = (S_Box[x][y][z] & 0x8) >> 3;
        *(Out + OutWordLeftBound + 1) = (S_Box[x][y][z] & 0x4) >> 2;
        *(Out + OutWordLeftBound + 2) = (S_Box[x][y][z] & 0x2) >> 1;
        *(Out + OutWordLeftBound + 3) = (S_Box[x][y][z] & 0x1);
    }
}
/******F function begin*******/
void E(ElemType* In, ElemType* Out) // 'In': ElemType [32], 'Out': ElemType [48]
{
    // The Expand, In = R_i[32], Out = ElemType[48] wait to be XOR with SubKey
    int i;
    for(i = 0; i < 48; ++i) // 'i' is the number of bit that we have changed.
        Out[i] = In[E_Box[i]-1];
}

void P(ElemType* In, ElemType* Out) // 'In': ElemType [32], 'Out': ElemType [32]
{
    // F function, In = S_Box's output, Out = Wait to be XOR with L_i
    int i;
    for(i = 0; i < 32; ++i)
        Out[i] = In[P_table[i]-1];
}

void F(ElemType* R, ElemType* Subkey, ElemType* Out) // 'In_1': ElemType [32], 'In_2':ElemType [48], 'Out': ElemType [32]
{
    // F function,   Out = Wait to be XOR with L_i
    ElemType Eout[48];
    ElemType SBoxout[32];

    E(R, Eout);
    Substitution(XOR(Eout, Subkey,48), SBoxout);
    P(SBoxout, Out);
}

/********Function ends*****/
void Round(ElemType* L, ElemType* R,ElemType* subkey) // 'InL', 'InR', 'OutL', 'OutR': ElemType [32], 'subkey': ElemType [48]
{
    int i;
    ElemType* temp;
    ElemType Fout[32];

    for(i=0; i!=16; ++i)
    {
        F(R,subkey+i*48,Fout);
        temp=XOR(L,Fout,32);
        L=R;
        R=temp;
    }
}

void IP(ElemType* In, ElemType* Out)
{
    // 'In': ElemType [16], 'Out': ElemType [16]
    // IP Transform, In = Message, Out = L_0|R_0
    int i;
    for(i = 0; i < 64; ++i) // 'i' is the number of bit that we have changed.
        Out[i]=In[IP_Box[i]-1];
#ifndef DES_RELEASE
    printf("IP layout\n");
    printHex(Out, 16);
#endif
}

void FP(ElemType* In, ElemType* Out)
{
    // 'In': ElemType [64], 'Out': ElemType [64]
    // FP Transform, In = L_16|R_16, Out = Cipher
    int i;
    for(i = 0; i < 64; ++i) // 'i' is the number of bit that we have changed.
    {
        Out[i]=In[FP_Box[i]-1];
    }
}

void DES_body(ElemType* block, ElemType* subkey, ElemType* out)//input: elemtype[64], key: elemtype[64]
{
    ElemType IPout[64],L[32],R[32];
    IP(block,IPout);
    memcpy(L,IPout,32);
    memcpy(R,IPout+32,32);
    Round(L,R,subkey);
    //We have to swap L and R on the last round.
    //So we can copy them on opposite side.
    memcpy(IPout,R,32);
    memcpy(IPout+32,L,32);
    FP(IPout,out);
}

/*input and key: char [64], output: char[64]*/
void DES_block_encrypt(char* input,char* key,char* output)
{
    char subkey[16*48];
    MakeSubKey(key,subkey);
    DES_body(input,subkey,output);
}

void DES_block_decrypt_Subkey_Reverse(char* subkey, char* rev_subkey)
{
    int i;
    for(i=0; i!=16; ++i)
        memcpy(rev_subkey+i*48,subkey+(15-i)*48,48);
}


void DES_block_decrypt(char* input,char* key,char* output)
{
    char subkey[16*48],rev_subkey[16*48];
    MakeSubKey(key,subkey);
    DES_block_decrypt_Subkey_Reverse(subkey,rev_subkey);
    DES_body(input,rev_subkey,output);
}


void DES_test()
{
    char datablock[]="123456789ABCDEEF", keybyte[]="123456789ABCDEEF";

    //char datablock[64],key[64];
    char outputbit[64],decryptbit[64];
    char databit[64],key[64];
    //Process Hex to bit stream(char [64])
    HexToStr(datablock,databit);
    HexToStr(keybyte,key);

    DES_block_encrypt(databit,key,outputbit);
    DES_block_decrypt(outputbit,key,decryptbit);

    //Print the result
    printHex(outputbit, 16);
    printHex(decryptbit, 16);
}

int main()
{
    DES_test();
    return 0;
}

