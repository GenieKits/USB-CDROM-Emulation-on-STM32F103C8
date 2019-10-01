#include <stdio.h>
#include <string.h>

#define CDROM_SECTOR_SIZE 2048

static unsigned char isoIOBuffer[CDROM_SECTOR_SIZE];
static int total; /*Total sectors need to be retained in the trimmed iso file.*/

static int ideRead(FILE *isofile, unsigned long lba, unsigned char *buffer)
{
  fseek(isofile, lba*CDROM_SECTOR_SIZE, SEEK_SET);
  if (fread(buffer, CDROM_SECTOR_SIZE, 1, isofile))
    return 0;
  return -1;
}

static int parseISO9660FileSystemDir(FILE *isofile,unsigned long lba,unsigned char *buf8,int depth)
{
  unsigned long offset;
  unsigned long fileLBA;
  unsigned long filesize,sectors;
  int __depth,i,filenameLength,isDir = 0,needRead = 0;
  char filename[2048 + 1];

  if(-1 == ideRead(isofile,lba,buf8))
    return -1;

  for(offset = 0; ; offset += buf8[offset + 0] /*Length of Directory Record.*/)
  {
    while(offset >= CDROM_SECTOR_SIZE)
    {
      offset -= CDROM_SECTOR_SIZE;
      ++lba; needRead = 1; /*Read again.*/
    }
    if(needRead)
    {
      if(ideRead(isofile,lba,buf8))
        return -1;
      needRead = 0;
    }

    if(buf8[offset + 0] == 0) /*Not any more.*/
      break;

    /*Location of extent (LBA) in both-endian format.*/
    fileLBA = *(unsigned long *)(buf8 + offset + 0x2);
    if(fileLBA <= lba)
      continue;

    /*Format the information printing.*/
    __depth = depth;
    while(__depth--)
      printf("--");

    isDir = buf8[offset + 25] & 0x2; /*Is it a dir?*/

    filesize = *(unsigned long *)(buf8 + offset + 10);
    sectors = filesize / CDROM_SECTOR_SIZE;
    if (filesize % CDROM_SECTOR_SIZE) sectors++;

    /* Length of file identifier (file name).
     * This terminates with a ';' character
     * followed by the file ID number in ASCII coded decimal ('1').*/
    filenameLength = buf8[offset + 32];
    if(!isDir) /*If it is a file...*/
      filenameLength -= 2; /*Remove ';' and '1'.*/

    memcpy((void *)filename,(const void *)(buf8 + offset + 33),filenameLength);

    if((!isDir) && (filename[0] == '_'))
      filename[0] = '.';
    if((!isDir) && (filename[filenameLength - 1] == '.'))
      filename[filenameLength - 1] = '\0';
    else
      filename[filenameLength] = '\0';

    /*To lower case.*/
    for(i = 0;i < filenameLength;++i)
      if((filename[i] <= 'Z') && (filename[i] >= 'A'))
        filename[i] -= 'A' - 'a';

    if(isDir)
    {
      printf("LBA:%d.",(int)fileLBA);
      printf("Dirname:%s\n",filename);
      parseISO9660FileSystemDir(isofile,fileLBA,buf8,depth + 1);
      ideRead(isofile,lba,buf8); /*We must read again.*/
    }
    else
    {
      if(filesize != 0)
        printf("LBA:%d.",(int)fileLBA);
      else
        printf("Null file, invalid LBA.");

      printf("Filename:%s,Filelength:%d,Sectors:%d\n",filename,filesize,sectors);
    }
  }

  total = fileLBA+sectors;
  printf("Total Sectors:%d\n",total);

  return 0;
}

static int parseISO9660FileSystem(FILE *isofile)
{
  /*See also http://wiki.osdev.org/ISO_9660.*/
  /*And http://en.wikipedia.org/wiki/ISO_9660.*/
  unsigned char *buf8  = (unsigned char  *)isoIOBuffer;

  /*System Area (32,768 B) is unused by ISO 9660*/
  /*32786 = 0x8000.*/
  unsigned long lba = (0x8000 / CDROM_SECTOR_SIZE);

  for(;;++lba)
  {
    if(ideRead(isofile,lba,buf8))
      return -1; /*It may not be inserted if error.*/

    /*Identifier is always "CD001".*/
    if( buf8[1] != 'C' ||
        buf8[2] != 'D' ||
        buf8[3] != '0' ||
        buf8[4] != '0' ||
        buf8[5] != '1' )
      return -1;

    if(buf8[0] == 0xff) /*Volume Descriptor Set Terminator.*/
      return -1;

    if(buf8[0] != 0x01 /*Primary Volume Descriptor.*/)
      continue;

    /*The entry for the root directory.*/
    if(buf8[156] != 0x22 /*Must be 34.*/)
      return -1;

    /*Location of extent (LBA) in both-endian format.*/
    lba = *(unsigned long *)(buf8 + 156 + 2);
    break;
  }

  /*Analyze the file structure of ROOT Dir.*/
  return parseISO9660FileSystemDir(isofile,lba,buf8,0);
}

int trimISO9660ImageSize(FILE *isofile)
{
  FILE * fp;

  fp = fopen("trimmed.iso","wb+");
  if (fp)
  {
    int i;
    for(i=0; i<=total; i++)
    {
      fseek(isofile, i*CDROM_SECTOR_SIZE, SEEK_SET);
      fread(isoIOBuffer, CDROM_SECTOR_SIZE, 1, isofile);
      fwrite(isoIOBuffer,CDROM_SECTOR_SIZE, 1, fp);
    }
    fclose(fp);
  }
  return 1;
}

int main(void)
{
  FILE * fp;

  fp = fopen("d:\\myworks\\test.iso", "rb");
  if (fp)
  {
    parseISO9660FileSystem(fp);
    trimISO9660ImageSize(fp);
    fclose(fp);
  }

  return 0;
}
