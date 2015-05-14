bin2c - A simple utility for converting a binary file to a c application which
can then be included within an application.

Usage:

bin2c input_file output_file array_name

for example, using:
    bin2c my_file.dat my_file.h data

will create something along the lines of

   const char data[3432] = {
       0x43, 0x28, 0x41, 0x11, 0xa3, 0xff,
       ...
       0x00, 0xff, 0x23
   };
   
   const int data_length = 3432;

This can then be used within your application, for example with SDL you would
use SDL_RWops. The application can also be used in a very similar fashion to
Qt's RC system.

I haven't included a Makefile because the utility is SO simple, I don't
think that one is needed. But for an example, compiling for GNU/Linux can be
done as shown

   gcc -o bin2c bin2c.c

In the current system, you can tell bin2c to compress the data with BZ2
compression. This would be very useful in applications where a lot of files
are stored this way or if memory is tight (although not CPU). To produce an
executable which can make bz2 files, define USE_BZ2. However, since this is
such a simple application, you can either define USE_BZ2 or not and it will
then produce compressed data or not. An example as to how to compile a BZ2
compression version of bin2c is as such

    gcc -o bin2cbz2 bin2c.c -DUSE_BZ2 -lbz2

This will add an extra constant, data_length_uncompressed, which is the size
of the file before it was compressed. So to decompress the file, you would
do something like the following:

    unsigned int decompressed_size = data_length_uncompressed;
    char *buf = malloc(data_length_uncompressed);
    int status;

    status = BZ2_bzBuffToBuffDecompress(buf, &decompressed_size,
            const_cast<char *>data, (unsigned int)data_length, 0, 0);

    // do something with buf
    free(buf);

I'm not entirely happy with having to do const_cast in C++ so if anyone can
suggest an alternative then I'd be happy to implement it.

Patches are welcome, just fork the project on github and send me a pull
request. If you are unable or unwilling to do this through github, then feel
free to email me your patch. This utility is so small I don't think that any
licence is needed, and I took most of the code from Serge Fukanchick and made
quite a few modifications so left it in the public domain. So please just send 
me a little note to say that you don't mind your code being in the public 
domain.
