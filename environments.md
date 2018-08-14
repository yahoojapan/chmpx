---
layout: contents
language: en-us
title: Environments
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: environmentsja.html
lang_opp_word: To Japanese
prev_url: developer.html
prev_string: Developer
top_url: index.html
top_string: TOP
next_url: tools.html
next_string: Tools
---

# Environments
CHMPX reads the following environment variables at startup.
## CHMDBGMODE
Corresponding option -d  
As with the -d option, you can specify the debug level. It can be used to change the debug level from the outside by debugging the program linking libchmpx.so. If the -d option is specified, the option takes precedence.
## CHMDBGFILE
Corresponding option -dfile  
You can specify the same debug output file as the -dfile option. If the -dfile option is specified, the option takes precedence.
## CHMCONFFILE
Corresponding option -conf  
-You can specify the same configuration file path as the -conf option. If the -conf / -json option is specified, the option takes precedence.
## CHMJSONCONF
Corresponding option -json  
As with the -json option, you can specify the configuration as a JSON string. If the -conf / -json option or the CHMCONFFILE environment variable is specified, the option or environment variable takes precedence.
