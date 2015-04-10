#!/bin/bash

if [ "$HOSTNAME" != "fdr1" ]; then

ssh claudeb@fdr1 'rm -rf ~/Debugging/InfinityVerbsTesting/; rm -rf ~/Debugging/InfinityVerbs/;'

ssh claudeb@fdr1 'mkdir -p ~/Debugging/; mkdir -p ~/Debugging/InfinityVerbs/; mkdir -p ~/Debugging/InfinityVerbs/Release/; mkdir -p ~/Debugging/InfinityVerbs/src/;'
rsync -r ~/Development/High-Speed-Networking/InfinityVerbs/src/ claudeb@fdr1:~/Debugging/InfinityVerbs/src/
rsync -r ~/Development/High-Speed-Networking/InfinityVerbs/Release/  claudeb@fdr1:~/Debugging/InfinityVerbs/Release/ --exclude="*.a" --exclude="*.d" --exclude="*.o"
ssh claudeb@fdr1 'cd ~/Debugging/InfinityVerbs/; find -exec touch \{\} \;'

ssh claudeb@fdr1 'mkdir -p ~/Debugging/; mkdir -p ~/Debugging/InfinityVerbsTesting/; mkdir -p ~/Debugging/InfinityVerbsTesting/Release/; mkdir -p ~/Debugging/InfinityVerbsTesting/src/;'
rsync -r ~/Development/High-Speed-Networking/InfinityVerbsTesting/src/ claudeb@fdr1:~/Debugging/InfinityVerbsTesting/src/
rsync -r ~/Development/High-Speed-Networking/InfinityVerbsTesting/Release/  claudeb@fdr1:~/Debugging/InfinityVerbsTesting/Release/ --exclude="*.a" --exclude="*.d" --exclude="*.o"
ssh claudeb@fdr1 'cd ~/Debugging/InfinityVerbsTesting/; find -exec touch \{\} \; ; sleep 2;'

ssh claudeb@fdr1 'rm ~/Debugging/InfinityVerbs/Release/libInfinityVerbs.a;'
ssh claudeb@fdr1 'cd ~/Debugging/InfinityVerbs/Release/; make all;'

ssh claudeb@fdr1 'rm ~/Debugging/InfinityVerbsTesting/Release/InfinityVerbsTesting;'
ssh claudeb@fdr1 'cd ~/Debugging/InfinityVerbsTesting/Release/; make all;'

ssh claudeb@fdr1 'cp ~/Debugging/InfinityVerbsTesting/Release/InfinityVerbsTesting ~/Debugging/ibdebug;'

fi

