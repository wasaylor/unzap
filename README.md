# unzap
A tool to extract files from the resource bundles (.ZAP) for the game _Star Wars: Bounty Hunter_ (PS2.)

I reverse engineered the file format in xxd and the decompression algorithm from the game executable in IDA.

## building
A very simple Makefile is included

## usage
`unzap [dir] <resource bundle>` will extract all files in the resource bundle to the working directory or to _dir_ if it's specified.
