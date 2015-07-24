A c++ library for parsing the adobe f4m manifest based on the available specs:
f4m ver 2.0
http://sourceforge.net/apps/mediawiki/osmf.adobe/index.php?title=Flash_Media_Manifest_%28F4M%29_File_Format_obsolete
f4m ver 3.0
http://www.adobe.com/devnet/hds.html

It depends on pugixml, use c++11 features, and need you to provide a callback function for the downloading part.

The interface is in f4mparser.h and manifest.h

I was unable to find a f4m parser so I decided to release it under the LGPL2 license if somebody want to use or improve it... I think you can occupy yourself quite a bit if you choose this way.