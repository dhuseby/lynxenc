#include <stdio.h>

/*
  Curt Vendell has posted the encryption sources to AtariAge.
  The encryption sources work by indexing everything with the
  least significant byte first.

  In the real Atari Lynx hardware the byte order is LITTLE_ENDIAN.
  If you run this on Intel or AMD CPU then you also have LITTLE_ENDIAN.
  But the original encryption was run on Amiga that has a BIG_ENDIAN CPU.

  This means that all the keys are presented in BIG_ENDIAN format.
*/

#define chunkLength 51
int ptr, c,
    num2, num7,
    ptr5, Cptr, Actr, Xctr, carry, err, ptrEncrypted;
unsigned char buffer[600];
unsigned char result[600];
static unsigned char A[chunkLength];
static unsigned char B[chunkLength];
static unsigned char InputData[chunkLength];
static unsigned char C[chunkLength];
static unsigned char PrivateKey[chunkLength];
static unsigned char E[chunkLength];
static unsigned char F[chunkLength];

#define BIT(C, i, m) ((C)[(i)/8] & (1 << (7 - ((i) & 7))))

void WriteOperand(FILE * fp, unsigned char *A, int m)
{
    int i;
    unsigned char byte;

    for (i = 0; i < m; i++) {
	byte = A[i];
	fprintf(fp, "%02x", byte);
    }
    fprintf(fp, "\n");
}

/* A = 0 */
static void Clear(unsigned char *A, int m)
{
    int i;

    for (i = 0; i < m; i++)
	A[i] = 0;
}

/* A = 1 */
static void One(unsigned char *A, int m)
{
    Clear(A, m);
    A[m - 1] = 1;
}

/* A = B */
static void Copy(unsigned char *A, unsigned char *B, int m)
{
    int i;

    for (i = 0; i < m; i++)
	A[i] = B[i];
}

/* B = 2*B */
static void Double(unsigned char *B, int m)
{
    int i, x;

    x = 0;
    for (i = m - 1; i >= 0; i--) 
    {
	    x += 2 * B[i];
	    B[i] = (unsigned char) (x & 0xFF);
	    x >>= 8;
    }
    /* shouldn't carry */
}

/* B = (B-N) if B >= N */
static int Adjust(unsigned char *B, unsigned char *PublicKey, int m)
{
    int i, x;
    unsigned char T[chunkLength];

    x = 0;
    for (i = m - 1; i >= 0; i--) 
    {
	    x += B[i] - PublicKey[i];
	    T[i] = (unsigned char) (x & 0xFF);
	    x >>= 8;
    }

    if (x >= 0) {
	Copy(B, T, m);
        return 1;
    }
    return 0;
}

/* v = -1/PublicKey mod 256 */
static void MontCoeff(unsigned char *v, unsigned char *PublicKey, int m)
{
    int i;
    int lsb = m - 1;

    *v = 0;
    for (i = 0; i < 8; i++)
	if (!((PublicKey[lsb] * (*v) & (1 << i))))
	    *v += (1 << i);
}

/* A = B*(256**m) mod PublicKey */
static void Mont(unsigned char *A, unsigned char *B, unsigned char *PublicKey,
		 int m)
{
    int i;

    Copy(A, B, m);

    for (i = 0; i < 8 * m; i++) {
	Double(A, m);
	Adjust(A, PublicKey, m);
    }
}

/* A = B*C/(256**m) mod PublicKey where v*PublicKey = -1 mod 256 */
static void MontMult(unsigned char *A, unsigned char *B, unsigned char *C,
		     unsigned char *PublicKey, unsigned char v, int m)
{
    int i, j;
    unsigned char ei, T[2 * chunkLength];
    unsigned int x;

    Clear(T, 2 * m);

    for (i = m - 1; i >= 0; i--) {
	x = 0;
	for (j = m - 1; j >= 0; j--) {
	    x += (unsigned int) T[i + j] +
		(unsigned int) B[i] * (unsigned int) C[j];
	    T[i + j] = (unsigned char) (x & 0xFF);
	    x >>= 8;
	}
	T[i] = (unsigned char) (x & 0xFF);
    }

    for (i = m - 1; i >= 0; i--) {
	x = 0;
	ei = (unsigned char) (((unsigned int) v * (unsigned int) T[m + i]) &
			      0xFF);
	for (j = m - 1; j >= 0; j--) {
	    x += (unsigned int) T[i + j] +
		(unsigned int) ei *(unsigned int) PublicKey[j];
	    T[i + j] = (unsigned char) (x & 0xFF);
	    x >>= 8;
	}
	A[i] = (unsigned char) (x & 0xFF);
    }

    x = 0;
    for (i = m - 1; i >= 0; i--) {
	x += (unsigned int) T[i] + (unsigned int) A[i];
	A[i] = (unsigned char) (x & 0xFF);
	x >>= 8;
    }
    /* shouldn't carry */
}

/* A = (B**PrivateKey)/(256**((PrivateKey-1)*m)) mod PublicKey, where v*PublicKey = -1 mod 256 */
static void MontExp(unsigned char *A, unsigned char *B, unsigned char *PrivateKey,
		    unsigned char *PublicKey, unsigned char v, int m)
{
    int i;
    unsigned char T[chunkLength];

    One(T, m);
    Mont(T, T, PublicKey, m);

    for (i = 0; i < 8 * m; i++) {
	MontMult(T, T, T, PublicKey, v, m);
	if (BIT(PrivateKey, i, m))
	    MontMult(T, T, B, PublicKey, v, m);
    }

    Copy(A, T, m);
}

/* A = B/(256**m) mod PublicKey, where v*PublicKey = -1 mod 256 */
static void UnMont(unsigned char *A, unsigned char *B, unsigned char *PublicKey,
		   unsigned char v, int m)
{
    unsigned char T[chunkLength];

    One(T, m);
    MontMult(A, B, T, PublicKey, v, m);

    Adjust(A, PublicKey, m);
}

/* All operands have least significant byte first. */
/* A = B**PrivateKey mod PublicKey */
void ModExp(unsigned char *A, unsigned char *B, unsigned char *PrivateKey,
	    unsigned char *PublicKey, int m)
{
    unsigned char T[chunkLength], v;

    MontCoeff(&v, PublicKey, m);
    Mont(T, B, PublicKey, m);
    MontExp(T, T, PrivateKey, PublicKey, v, m);
    UnMont(A, T, PublicKey, v, m);
}

/*
    The inner working of the Lynx. Code created by Harry Dodgson by analyzing
    the Lynx disassembled code
*/
// This is the known public key from the Lynx ROM
// Please note that this key is actually BIG_ENDIAN
// even if it inside the Lynx that is LITTLE_ENDIAN
static unsigned char LynxPublicKey[chunkLength] = {
    0x35, 0xB5, 0xA3, 0x94, 0x28, 0x06, 0xD8, 0xA2,
    0x26, 0x95, 0xD7, 0x71, 0xB2, 0x3C, 0xFD, 0x56,
    0x1C, 0x4A, 0x19, 0xB6, 0xA3, 0xB0, 0x26, 0x00,
    0x36, 0x5A, 0x30, 0x6E, 0x3C, 0x4D, 0x63, 0x38,
    0x1B, 0xD4, 0x1C, 0x13, 0x64, 0x89, 0x36, 0x4C,
    0xF2, 0xBA, 0x2A, 0x58, 0xF4, 0xFE, 0xE1, 0xFD,
    0xAC, 0x7E, 0x79
};


// B = B + F
void add_it(unsigned char *B, unsigned char *F, int m)
{
    int ct, tmp;
    carry = 0;
    for (ct = m - 1; ct >= 0; ct--) {
	tmp = B[ct] + F[ct] + carry;
	if (tmp >= 256)
	    carry = 1;
	else
	    carry = 0;
	B[ct] = (unsigned char) (tmp);
    }
}

/* A = B*(256**m) mod PublicKey */
static void LynxMontWorks(unsigned char *A1, unsigned char *B1, unsigned char *PublicKey,
		 int m)
{
    int Yctr;

    Clear(B, m);
    Yctr = 0;
    do {
	int num8, numA;
	numA = F[Yctr];
	num8 = 255;
	do {
	    Double(B, m);
	    carry = (numA & 0x80) / 0x80;
	    numA = (unsigned char) (numA << 1);
	    if (carry != 0) {
		add_it(B, E, m);
                carry = Adjust(B, PublicKey, m);
		if (carry != 0)
                    Adjust(B, PublicKey, m);
	    } else
                Adjust(B, PublicKey, m);
	    num8 = num8 >> 1;
	} while (num8 != 0);
	Yctr++;
    } while (Yctr < m);
}

/* A = B*(256**m) mod PublicKey */
static void LynxMont(unsigned char *A1, unsigned char *B1, unsigned char *PublicKey,
		 int m)
{
    int Yctr;

    Clear(B, m);
    Yctr = 0;
    do {
	int num8, numA;
	numA = F[Yctr];
	num8 = 255;
	do {
	    Double(B, m);
	    carry = (numA & 0x80) / 0x80;
	    numA = (unsigned char) (numA << 1);
	    if (carry != 0) {
		add_it(B, E, m);
                carry = Adjust(B, PublicKey, m);
		if (carry != 0)
                    Adjust(B, PublicKey, m);
	    } else
                Adjust(B, PublicKey, m);
	    num8 = num8 >> 1;
	} while (num8 != 0);
	Yctr++;
    } while (Yctr < m);
}

void sub5000(int m)
{
    Copy(F, E, m);
    LynxMont(B, E, LynxPublicKey, m);
    Copy(F, B, m);
    LynxMont(B, E, LynxPublicKey, m);
}

void convert_it()
{
    int ct;
    long t1, t2;

    num7 = buffer[Cptr];
    num2 = 0;
    Cptr++;
    do {
	int Yctr;

	for (ct = chunkLength - 1; ct >= 0; ct--) {
	    E[ct] = buffer[Cptr];
	    Cptr++;
	}
	if ((E[0] | E[1] | E[2]) == 0) {
	    err = 1;
	}
	t1 = ((long) (E[0]) << 16) +
	    ((long) (E[1]) << 8) +
	    (long) (E[2]);
	t2 = ((long) (LynxPublicKey[0]) << 16) +
	    ((long) (LynxPublicKey[1]) << 8) + (long) (LynxPublicKey[2]);
	if (t1 > t2) {
	    err = 1;
	}
	sub5000(chunkLength);
	if (B[0] != 0x15) {
	    err = 1;
	}
	Actr = num2;
	Yctr = 0x32;
	do {
	    Actr += B[Yctr];
	    Actr &= 255;
	    result[ptr5] = (unsigned char) (Actr);
	    ptr5++;
	    Yctr--;
	} while (Yctr != 0);
	num2 = Actr;
	num7++;
    } while (num7 != 256);
    if (Actr != 0) {
	err = 1;
    }
}

// This is what really happens inside the Atari Lynx at boot time
void LynxDecrypt(unsigned char encrypted_data[])
{
    int i;

    ptrEncrypted = 0xAA;
    c = 410;
    
    // this copies the encrypted loader into the buffer
    for (i = 0; i < c; i++) 
    {
	    buffer[i] = encrypted_data[i];
    }

    ptr5 = 0;
    Cptr = 0;
    
    convert_it();
    
    ptr5 = 256;
    
    convert_it();
}

unsigned char AtariPrivateKey[chunkLength];
unsigned char Result[410];

void ReadLength(FILE * fp, int *m)
{
    fscanf(fp, "%d", m);
}

void ReadOperand(FILE * fp, unsigned char *A, int m)
{
    int i;
    unsigned int byte;

    for (i = m - 1; i >= 0; i--) {
	fscanf(fp, "%02x", &byte);
	A[i] = (unsigned char) byte;
    }
}

void CopyOperand(unsigned char *A, unsigned char *B, int m, char inverted)
{
    int i, j;

    if (inverted) {
        int j;
        j = 0;
        for (i = m - 1; i >= 0; i--) {
	    B[j++] = A[i];
        }
    } else {
        for (i = 0; i < m; i++) {
	    B[i] = A[i];
        }
    }
}

#define bool char
#define false 0
#define true 1

bool Compare(unsigned char *A, unsigned char *B, int m)
{
    int i;
    bool res = true;

    for (i = 0; i < m; i++) {
	if (B[i] != A[i])
	    res = false;
    }
    return res;
}

static unsigned char PublicKey[chunkLength];

// This is the known public exponent from the Lynx ROM
static unsigned char LynxExponent[chunkLength] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03
};

// This is the known public exponent from the Lynx ROM
static unsigned char LynxExponentInverted[chunkLength] = {
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};

// This is the Atari keyfile.1
// I believe it is the encrypted private key for signing carts
static unsigned char keyfile_1[chunkLength] = {
    0xea, 0x6c, 0xad, 0xb2, 0xab, 0xb1, 0xd3, 0xee,
    0x85, 0x6f, 0xd3, 0x36, 0xc0, 0xc1, 0x16, 0x1d,
    0x31, 0x44, 0x65, 0x1a, 0x22, 0x81, 0xb5, 0xb8,
    0x26, 0xdd, 0xce, 0x0f, 0x8f, 0xbb, 0x25, 0xc8,
    0x1d, 0x34, 0x03, 0x1f, 0xb4, 0xb9, 0xae, 0xda,
    0xcf, 0xde, 0x75, 0xc1, 0xd2, 0xed, 0x35, 0x4b,
    0xcc, 0x11, 0x58
};

// This is the Atari keyfile.2
// I believe it is the exponent for protecting the signing private key
static unsigned char keyfile_2[chunkLength] = {
    0x14, 0xd6, 0x30, 0x08, 0x35, 0x57, 0x28, 0xef,
    0x2b, 0xa3, 0x25, 0xb7, 0x11, 0x8c, 0x62, 0x2d,
    0x16, 0x7a, 0x7d, 0xee, 0x57, 0xe7, 0x37, 0x18,
    0xc9, 0x96, 0xe5, 0xa9, 0x63, 0x49, 0x68, 0x15,
    0xf6, 0x6c, 0x12, 0x8c, 0x9e, 0xeb, 0xda, 0xef,
    0xbd, 0x75, 0x3a, 0x9e, 0x7d, 0x02, 0xe6, 0xe9,
    0xfd, 0xd7, 0x97
};

// This is the Atari keyfile.3
// I believe it is the public key for protecting the signing private key
static unsigned char keyfile_3[chunkLength] = {
    0xdd, 0x74, 0xf0, 0xb7, 0xee, 0xe2, 0x6b, 0x6d,
    0xb7, 0x75, 0xcc, 0xca, 0x1d, 0x65, 0xdc, 0xd4,
    0x35, 0xe2, 0x09, 0xd0, 0x18, 0x46, 0x9b, 0xf5,
    0x96, 0xcc, 0x80, 0xfa, 0x44, 0xea, 0xee, 0x0e,
    0x23, 0xbb, 0x36, 0xfe, 0x68, 0x22, 0xbf, 0xb5,
    0x53, 0x7d, 0xf2, 0xfb, 0x86, 0x82, 0x94, 0x13,
    0xd4, 0x24, 0x6c
};

// The 410 byte miniloader created by Harry for the cc65 compiler
static unsigned char HarrysEncryptedLoader[410] = {
    0xFD, 0xC1, 0x0D, 0x8E, 0xE9, 0xEE, 0x09, 0x13,
    0xE5, 0x96, 0x0C, 0x34, 0x64, 0xDA, 0xD4, 0xBB,
    0x99, 0xEC, 0xCE, 0x4F, 0xAA, 0x8C, 0xED, 0x65,
    0xF0, 0x32, 0x70, 0xA3, 0x84, 0xC4, 0xFC, 0xA2,
    0x6D, 0x3A, 0xF8, 0x77, 0x4B, 0xAC, 0x9B, 0x54,
    0x7D, 0x82, 0x6F, 0xF8, 0xA5, 0x06, 0x4D, 0x7B,
    0x77, 0x55, 0xE4, 0x31, 0xC4, 0x2C, 0x2F, 0x2F,
    0xB6, 0x4D, 0x15, 0xA9, 0xC7, 0x99, 0x5D, 0x6E,
    0xB3, 0x97, 0x92, 0x44, 0x7B, 0x2B, 0x85, 0x18,
    0xE6, 0xF1, 0x96, 0xF4, 0xC4, 0xDE, 0xA4, 0xCF,
    0x79, 0xE2, 0xC1, 0x1A, 0xE0, 0x0C, 0x93, 0xC5,
    0x26, 0xBD, 0xA3, 0x16, 0x8A, 0xC3, 0x59, 0xA0,
    0x39, 0x38, 0xA0, 0x3B, 0xEF, 0xBB, 0x1D, 0x5C,
    0x0D, 0x1D, 0xCC, 0x48, 0x1D, 0xDD, 0x98, 0x9A,
    0x7A, 0xF7, 0x96, 0xF9, 0x61, 0x03, 0x50, 0xDA,
    0x47, 0x69, 0x94, 0xC3, 0x80, 0xDA, 0xA9, 0x99,
    0xA1, 0x21, 0x2B, 0x2E, 0x7D, 0xF5, 0xE4, 0xF7,
    0xB3, 0x5C, 0xA8, 0x14, 0xFA, 0xE9, 0x06, 0xAC,
    0x1E, 0x9F, 0xB5, 0x31, 0xBE, 0x42, 0x14, 0x08,
    0x0E, 0x05, 0xFB, 0x25, 0xBB, 0x5C, 0x5C, 0x66,
    0x76, 0x8E, 0x36, 0xE8, 0xEB, 0x39, 0xF2, 0x26,
    0xBD, 0x17, 0x29, 0xF4, 0xB8, 0x1D, 0x7E, 0xEE,
    0x47, 0x61, 0xBB, 0x9E, 0xF5, 0x72, 0xC9, 0xBC,
    0x26, 0x37, 0xD5, 0x78, 0x8F, 0xD0, 0xCE, 0x95,
    0x21, 0xEB, 0x4A, 0x07, 0x8D, 0x3A, 0x3A, 0x01,
    0x82, 0xCF, 0x01, 0xC5, 0x1E, 0x1D, 0xA8, 0x41,
    0x4F, 0xBD, 0xC1, 0x76, 0x22, 0xA3, 0x88, 0xD9,
    0x57, 0xC9, 0x51, 0x3A, 0x26, 0xBE, 0x4A, 0x1A,
    0x7F, 0x42, 0x61, 0xCF, 0xFC, 0xFC, 0x5B, 0x06,
    0x94, 0xD2, 0x2C, 0x78, 0x45, 0xBA, 0x93, 0xC4,
    0x7D, 0x7C, 0x81, 0x73, 0x07, 0x4F, 0xE2, 0x6C,
    0xE9, 0x81, 0x1A, 0xDE, 0x77, 0x74, 0x87, 0xDE,
    0x26, 0x9E, 0x7A, 0xA8, 0x19, 0xA7, 0x34, 0x32,
    0x70, 0xED, 0x59, 0xA8, 0x4A, 0xD8, 0xFE, 0xCB,
    0xDD, 0x02, 0x2F, 0xCE, 0x92, 0xE9, 0x13, 0xA6,
    0xFF, 0xB4, 0x4B, 0x18, 0x9D, 0x63, 0x48, 0xE0,
    0x3B, 0x3B, 0x0D, 0x2B, 0xFC, 0x04, 0xA4, 0xE3,
    0x5E, 0x4C, 0x3C, 0x94, 0x70, 0xC4, 0xF0, 0x64,
    0x15, 0x48, 0x68, 0x17, 0xDE, 0x14, 0x72, 0xF0,
    0x59, 0x33, 0x4C, 0x49, 0x47, 0x8D, 0xB6, 0xF4,
    0x82, 0x4E, 0xB7, 0x4E, 0x01, 0xC9, 0xC2, 0x82,
    0x0B, 0x7A, 0xAC, 0x67, 0x9B, 0x0F, 0x04, 0xE1,
    0xB6, 0x78, 0x34, 0xC8, 0x4F, 0x2A, 0x11, 0xED,
    0xD0, 0x1C, 0x6D, 0xCD, 0x3D, 0x47, 0x09, 0x8B,
    0xE5, 0x38, 0x19, 0x7A, 0x31, 0x6E, 0x30, 0x71,
    0x1C, 0x90, 0x34, 0xE5, 0x44, 0xCC, 0x00, 0xC7,
    0x41, 0xD0, 0x27, 0x8A, 0x06, 0x29, 0x5C, 0x2B,
    0xE4, 0x26, 0x63, 0x09, 0x52, 0xD3, 0x97, 0x33,
    0xD7, 0x59, 0x1C, 0x36, 0x2F, 0xC9, 0xA9, 0xA2,
    0xB5, 0xBB, 0xA9, 0x1D, 0xE6, 0x36, 0x7E, 0x56,
    0x05, 0xA4, 0x9C, 0xE0, 0x45, 0x59, 0x21, 0xE1,
    0xE6, 0x21
};

// This is Harrys miniloader in plaintext
static unsigned char HarrysPlaintextLoader[410] = {
    0x80, 0x00, 0x20, 0x4f, 0x02, 0x64, 0x05, 0xe6,
    0x06, 0xa9, 0x08, 0x8d, 0x8b, 0xfd, 0x4c, 0x4a,
    0xfe, 0xa2, 0x00, 0x20, 0x00, 0x03, 0xa2, 0x0b,
    0xbd, 0x6d, 0x02, 0xbc, 0x76, 0x02, 0x99, 0x00,
    0xfc, 0xca, 0xd0, 0xf4, 0x9c, 0x91, 0xfd, 0xa9,
    0x04, 0x8d, 0x95, 0xfd, 0xa0, 0x1f, 0xb9, 0x00,
    0x24, 0x99, 0xa0, 0xfd, 0x88, 0x10, 0xf7, 0x8a,
    0x9d, 0x00, 0x24, 0xe8, 0xd0, 0xf9, 0x4c, 0x49,
    0x03, 0x00, 0x7a, 0x02, 0x00, 0x24, 0x40, 0x1c,
    0x07, 0xba, 0x02, 0x00, 0x04, 0x64, 0x60, 0xa2,
    0x1f, 0x9e, 0xa0, 0xfd, 0xca, 0x10, 0xfa, 0xa9,
    0x04, 0x8d, 0x8c, 0xfd, 0xa9, 0x0f, 0x8d, 0x01,
    0x02, 0x60, 0xa0, 0x10, 0xad, 0xb2, 0xfc, 0x95,
    0x36, 0xe8, 0x88, 0xd0, 0xf7, 0x60, 0x01, 0x20,
    0x04, 0x00, 0x01, 0x00, 0x00, 0x24, 0x20, 0x91,
    0x92, 0x09, 0x08, 0x90, 0x04, 0x06, 0x11, 0x10,
    0x28, 0x2a, 0x47, 0x39, 0x00, 0x9d, 0x11, 0x8f,
    0x5e, 0xd9, 0x87, 0x94, 0x5e, 0xa7, 0x4e, 0xff,
    0xe7, 0x05, 0xba, 0xd1, 0x55, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x62, 0x02, 0xe6, 0x3c, 0xe6, 0x3d, 0xa5,
    0x36, 0x20, 0x00, 0xfe, 0xa6, 0x37, 0xe8, 0xa4,
    0x38, 0xc8, 0x20, 0x42, 0x03, 0x38, 0xa6, 0x37,
    0xa5, 0x38, 0xe9, 0x04, 0xa8, 0xc6, 0x3c, 0xd0,
    0x04, 0xc6, 0x3d, 0xf0, 0x19, 0xad, 0xb2, 0xfc,
    0x92, 0x3a, 0xe6, 0x3a, 0xd0, 0x02, 0xe6, 0x3b,
    0xe8, 0xd0, 0xea, 0xc8, 0xd0, 0xe7, 0xe6, 0x36,
    0x64, 0x37, 0x64, 0x38, 0x80, 0xc9, 0x60, 0xad,
    0xb2, 0xfc, 0xca, 0xd0, 0xfa, 0x88, 0xd0, 0xf7,
    0x60, 0xa9, 0x12, 0x85, 0x33, 0xa5, 0x33, 0x4a,
    0x4a, 0xe5, 0x33, 0x4a, 0x2e, 0x82, 0x02, 0x2e,
    0x83, 0x02, 0x66, 0x33, 0xa5, 0x33, 0x6d, 0x82,
    0x02, 0x4d, 0x83, 0x02, 0xa8, 0xbd, 0x00, 0x24,
    0x48, 0xb9, 0x00, 0x24, 0x9d, 0x00, 0x24, 0x68,
    0x99, 0x00, 0x24, 0xe8, 0xd0, 0xd7, 0xce, 0xf5,
    0x03, 0xd0, 0xd2, 0xa2, 0x32, 0x74, 0x00, 0xca,
    0xd0, 0xfb, 0xa5, 0x31, 0x20, 0x00, 0xfe, 0xad,
    0xb0, 0xfc, 0xf0, 0x03, 0x20, 0x4f, 0x02, 0xa9,
    0x10, 0x85, 0x32, 0x38, 0xa2, 0x10, 0xad, 0xb2,
    0xfc, 0xa0
};

void test(char first[51], char second[51], char third[51], bool inverted)
{
    int m;

    // Now we try to make the same thing work in the Amiga way
    // The first thing we need to do is to decrypt the file again
    // using the provided exponent and public key

    // This is what happens in the Amiga. It will read in the length and 3 keys.
    //ReadLength (stdin, &m);
    m = 51;
    Clear(InputData, m);
    Clear(PrivateKey, m);
    Clear(PublicKey, m);
    Clear(Result, m);

    //ReadOperand (stdin, InputData, m);
    CopyOperand(first, InputData, m, inverted);
    //ReadOperand (stdin, PrivateKey, m);
    CopyOperand(second, PrivateKey, m, inverted);
    //ReadOperand (stdin, PublicKey, m);
    CopyOperand(third, PublicKey, m, inverted);

    ModExp(Result, InputData, PrivateKey, PublicKey, m);

    if (Compare(Result, HarrysPlaintextLoader, 51)) {
	printf("Decrypt works\n");
    } else {
	printf("Decrypt fails\n");
        WriteOperand(stdout, InputData, m);
        WriteOperand(stdout, PrivateKey, m);
        WriteOperand(stdout, PublicKey, m);
        WriteOperand(stdout, Result, m);
        WriteOperand(stdout, HarrysPlaintextLoader, m);
    }
}

/* Computes A = InputData**PrivateKey mod PublicKey.
   (1) Inputs length in bytes of operands.
   (2) Inputs InputData, then PrivateKey, then PublicKey, most significant byte first. Most
       significant bit of most significant byte of PublicKey must be zero.
   (3) Computes A.
   (4) Outputs A, most significant byte first.
 */
int main(int argc, char *argv[])
{
    int m;

    LynxDecrypt(HarrysEncryptedLoader);
    if (Compare(result, HarrysPlaintextLoader, 410)) 
    {
    	printf("LynxDecrypt works\n");
    } 
    else 
    {
	    printf("LynxDecrypt fails\n");
    }

    return 0;
}
