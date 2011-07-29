..\bin\cabocha-model-index -f SHIFT-JIS -t %1 dep.ipa.txt dep.ipa.model
..\bin\cabocha-model-index -f SHIFT-JIS -t %1 chunk.ipa.txt chunk.ipa.model
..\bin\cabocha-model-index -f SHIFT-JIS -t %1 ne.ipa.txt ne.ipa.model
echo %1> charset-file.txt
