#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <ctype.h>


#define BMASK(x)				(1<<(x))

#define REQ_ARG_INPUT           (BMASK(0))
#define REQ_ARG_OUTPUT          (BMASK(1))
#define REQ_MASK                (REQ_ARG_INPUT | REQ_ARG_OUTPUT)

typedef enum{
    NO_ERROR = 0,
    ERROR_ARG_COUNT,
    ERROR_ARG_FORMAT,
    ERROR_ARG_LENGTH,
    ERROR_ALLOCATE_INPUT,
    ERROR_ALLOCATE_OUTPUT,
    ERROR_OPEN_FILE,
    ERROR_GET_FILE_SIZE,
    ERROR_FILE_SIZE,
    ERROR_ALLOCATE_DATA,
    ERROR_READ,
    ERROR_NOT_FOUND,
    ERROR_WRITE_FILE,
} RETURNCODE;


volatile const uint8_t vpatn[8] =
{
    0x48, 0x49, 0x44, 0x00, 0x12, 0xAB, 0x56, 0xEF,
	//VERSION_TEST_LETTER, VERSION_REVISION, VERSION_MINOR, VERSION_MAJOR,
	//PRODUCT_COMPONENT, PRODUCT_REV,	PRODUCT_ID,  PRODUCT_GEN,
};



char *inName = NULL;
char *outName = NULL;
FILE *inFile = NULL;
FILE *outFile = NULL;
uint8_t *elfFile = NULL;

uint32_t fVersion = 0xffffffff; 
uint32_t fProduct = 0xffffffff; 


RETURNCODE parse_args(int argc, char* argv[]);
bool set_input_file(char *fName);
bool set_output_file(char *fName);


RETURNCODE open_file(char *name, const char *mode, FILE **f);
RETURNCODE dump_file(FILE *f, uint8_t **dst, uint32_t *size);
RETURNCODE find_id(uint8_t *src, uint32_t size);
RETURNCODE file_write(FILE *f, uint8_t *data, uint32_t nbytes);

int main(int argc, char *argv[])
{
    uint32_t inSize = 0;
//Parse Args
    RETURNCODE error = parse_args(argc, argv);

//Open Files
if(!error)
    error = open_file(inName, "rb", &inFile);
if(!error)
    error = open_file(outName, "wb", &outFile);

//dump file
if(!error)
    error = dump_file(inFile, &elfFile, &inSize);

//find info
if(!error)
    error = find_id(elfFile, inSize);

//write file
if(!error)
    error = file_write(outFile, (uint8_t *) &fVersion, 4);
if(!error)
    error = file_write(outFile, (uint8_t *) &fProduct, 4);

//Free memory
    if(inName)
    {
        free(inName);
    }
    if(outName)
    {
        free(outName);
    }
    if(inFile)
    {
        fclose(inFile);
    }
    if(outFile)
    {
        fclose(outFile);
    }
    if(elfFile)
    {
        free(elfFile);
    }
//Output Success if done
    if(!error)
    {
        printf("Success\n");
    }

    return error;
}


RETURNCODE parse_args(int argc, char* argv[])
{
    if(argc < 3)
        return ERROR_ARG_COUNT;

    uint32_t required = 0;

    //argv[0] = the executable so skip in the for loop
    for(int i = 1; i < argc; i++)
    {
        if(argv[i][0] != '-')
            return ERROR_ARG_FORMAT;

        if(strlen(argv[i]) <= 3)
            return ERROR_ARG_LENGTH;

        if(argv[i][2] != '=')
            return ERROR_ARG_FORMAT;

        char opt = argv[i][1];
        char* arg = &argv[i][3];
        switch(opt)
        {
            case 'i':
            {
                if(!set_input_file(arg))
                    return ERROR_ALLOCATE_INPUT;
                required |= REQ_ARG_INPUT;
                break;
            }
            case 'o':
            {
                if(!set_output_file(arg))
                    return ERROR_ALLOCATE_OUTPUT;
                required |= REQ_ARG_OUTPUT;
                break;
            }
            default:
            {
                return ERROR_ARG_FORMAT;
            }
        }
    }

    if(required != REQ_MASK)
        return ERROR_ARG_FORMAT;

    return NO_ERROR;
}

//Allocate and store Input File Name
bool set_input_file(char *fName)
{
    int inNameSize = strlen(fName);
    inName  = static_cast<char*> (malloc(inNameSize + 1));
    if(inName == NULL)
        {
           return false;
        }
    else
        {
        strcpy(inName, fName);
        }

    return true;
}

//Allocate and store Output File Name
bool set_output_file(char *fName)
{
    int outNameSize = strlen(fName);
    outName  = static_cast<char*> (malloc(outNameSize + 1));
    if(outName == NULL)
        {
           return false;
        }
    else
        {
        strcpy(outName, fName);
        }

    return true;
    
}


//open input file
RETURNCODE open_file(char *name, const char *mode, FILE **f)
{
    *f = fopen( name, mode);
    if(!*f)
    {
        printf("Error: Unable to open %s\n", name);
        return ERROR_OPEN_FILE;
    }
    return NO_ERROR;
}

RETURNCODE dump_file(FILE *f, uint8_t **dst, uint32_t *size)
{
    *size = 0;

    if(fseek(f, 0, SEEK_END) != 0)
    {
        return ERROR_GET_FILE_SIZE;
    }

    *size = ftell(f);
    rewind(f);

    if(*size == 0)
        return ERROR_FILE_SIZE;
    
    *dst = static_cast<uint8_t*> (malloc(*size));

    if(*dst == NULL)
        return ERROR_ALLOCATE_DATA;

    size_t rbytes = fread(static_cast<void *>(*dst), 1, *size, f);

    if(rbytes != *size)
        return ERROR_READ;

    return NO_ERROR;
}

RETURNCODE find_id(uint8_t *src, uint32_t size)
{
    uint32_t check;
    bool match;

    for(uint32_t i = 0; i < (size-8); i++)
    {
        check = 0;
        match = true;

        while(match && (check < 8))
        {
            if(src[i+check] != vpatn[check])
            {
                match = false;
            }
            check++;
        }

        if(match)
        {
            uint32_t val;
            uint8_t *buf = (uint8_t *) &val;
            buf[0] = src[i+8];
            buf[1] = src[i+9];
            buf[2] = src[i+10];
            buf[3] = src[i+11];
            fVersion = val;
            buf[0] = src[i+12];
            buf[1] = src[i+13];
            buf[2] = src[i+14];
            buf[3] = src[i+15];
            fProduct = val;
            break;
        }
    }

    if(!match)
        return ERROR_NOT_FOUND;

    return NO_ERROR;
}

RETURNCODE file_write(FILE *f, uint8_t *data, uint32_t nbytes)
{
    uint32_t nWritten;
    nWritten = fwrite(data, 1, nbytes, f);
    if(nWritten != nbytes)
        {
            return ERROR_WRITE_FILE;
        }
    return NO_ERROR;
}