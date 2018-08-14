---
layout: contents
language: ja
title: Environments
short_desc: Consistent Hashing Mq inProcess data eXchange
lang_opp_file: environments.html
lang_opp_word: To English
prev_url: developerja.html
prev_string: Developer
top_url: indexja.html
top_string: TOP
next_url: toolsja.html
next_string: Tools
---

# 環境変数
CHMPXは起動時に以下の環境変数を読み込みます。
## CHMDBGMODE
対応するオプション -d  
-dオプションと同じくデバッグレベルを指定することができます。libchmpx.soをリンクするプログラムのデバッグなどで外部からデバッグレベルを変更するために利用できます。　-dオプションが指定されている場合にはオプションが優先されます。
## CHMDBGFILE
対応するオプション -dfile  
-dfileオプションと同じデバッグ出力ファイルを指定できます。 -dfileオプションが指定されている場合にはオプションが優先されます。
## CHMCONFFILE
対応するオプション -conf  
-confオプションと同じコンフィグレーションファイルパスを指定できます。 -conf / -jsonオプションが指定されている場合にはオプションが優先されます。
## CHMJSONCONF
対応するオプション -json  
-jsonオプションと同様にコンフィグレーションをJSON文字列で指定することができます。　-conf / -jsonオプションもしくは CHMCONFFILE環境変数が指定されている場合にはオプションもしくは環境変数が優先されます。
