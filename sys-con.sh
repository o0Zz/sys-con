#!/bin/bash
# Script dependencies
# ---------------------
# pacman -S lftp
#
# This script is used to simplify nintendo switch developement
# You need sys-ftpd installed
# Boot the switch, use this script to do basic actions:
# "ftp upload" to upload the binary (You just built) directly to the switch
# "ftp logs" to display the logs
# "ftp crash" to provide details about last fatal report crash
# Same commands are available with "sd", usefull when the nintendo switch crash on boot (You plug the sdcard to your laptop and debug from there)

# IP of your switch on your network:
FTP_URL=ftp://192.168.10.238:5000
FTP_USER=o0zz
FTP_PASS=rootroot

#Below variable need to stay unchanged, they are correct whatever the developer
ELF_FILE=./source/Sysmodule/sys-con.elf
NSP_FILE=./out/atmosphere/contents/690000000000000D/exefs.nsp
DST_NSP_FILE=atmosphere/contents/690000000000000D/exefs.nsp
NRO_FILE=./out/switch/sys-con.nro
DST_NRO_FILE=switch/sys-con.nro
LOG_FILE=config/sys-con/log.txt
REPORT_FOLDER=atmosphere/fatal_errors
DMP_FOLDER=config/sys-con

usage () {
  echo "Usage: "
  echo "       $0 ftp <upload|logs|crash|dmp>"
  echo "       $0 sd <upload|logs|crash|dmp>"
  echo "       $0 build"
  echo "       $0 stacktrace"
  exit 1
}

display_logs () {
	echo "---- LOGS ----"
	while IFS= read -r line
	do
		if [[ $line == "|I|"* ]]; then
			echo -e "\033[0;34m${line}\033[0m"
		elif [[ $line == "|E|"* ]]; then
			echo -e "\033[0;31m${line}\033[0m"
		elif [[ $line == "|W|"* ]]; then
			echo -e "\033[0;33m${line}\033[0m"
		else
			echo "$line"
		fi
	done < $1
    echo "---- END ----"
}

display_fatalerror () {
	if [ ! -f "AFE_Parser.exe" ]; then
		curl -L -o AFE_Parser.exe https://github.com/o0Zz/AFE_Parser/releases/download/v1.3.1/AFE_Parser.exe
	fi
	if [ ! $1 ]; then
		echo "*** No crash report ***"
		exit 1
	fi
	./AFE_Parser.exe -report $1 -elf $ELF_FILE
}


if [ "$1" == "ftp" ]; then
    if [ "$2" == "upload" ]; then
        echo "FTP upload"
        curl -u "$FTP_USER:$FTP_PASS" -T $NSP_FILE "$FTP_URL/$DST_NSP_FILE"  || exit 1
        #curl -u "$FTP_USER:$FTP_PASS" -T $NRO_FILE "$FTP_URL/$DST_NRO_FILE"  || exit 1
    elif [ "$2" == "logs" ]; then
        curl -u "$FTP_USER:$FTP_PASS" "$FTP_URL/$LOG_FILE" -o ./log.txt || exit 1
        display_logs ./log.txt
    elif  [ "$2" == "crash" ]; then
		echo "Downloading all .bin crash reports from $REPORT_FOLDER..."
		
		rm *.bin
		lftp -u "$FTP_USER","$FTP_PASS" "$FTP_URL" <<EOF
cd $REPORT_FOLDER
mget *.bin -O .
glob -a rm *.bin
bye
EOF

        display_fatalerror $(ls | grep -Eo '[^ ]+\.bin' | head -n 1)
    elif  [ "$2" == "dmp" ]; then
		echo "Downloading and deleting all .dmp files from $DMP_FOLDER..."
		
		rm *.dmp
		lftp -u "$FTP_USER","$FTP_PASS" "$FTP_URL" <<EOF
cd $DMP_FOLDER
mget *.dmp -O .
glob -a rm *.dmp
bye
EOF

	else
		usage
	fi
	
elif  [ "$1" == "sd" ]; then
    if [ "$2" == "upload" ]; then
        echo "SD card copy"
        cp  $NSP_FILE /d/$DST_NSP_FILE || exit 1
		#cp  $NRO_FILE /d/$DST_NRO_FILE || exit 1
        sync
        echo "OK"

    elif [ "$2" == "logs" ]; then
        cp  /d/$LOG_FILE ./log.txt || exit 1
        display_logs ./log.txt

    elif [ "$2" == "crash" ]; then
		echo "Downloading all .bin crash reports from $REPORT_FOLDER..."
		rm *.bin
		cp /d/$REPORT_FOLDER/*.bin .
		rm /d/$REPORT_FOLDER/*.bin
		
        display_fatalerror $(ls | grep -Eo '[^ ]+\.bin' | head -n 1)
	elif  [ "$2" == "dmp" ]; then
		echo "Downloading and deleting all .dmp files from $DMP_FOLDER..."
		
		rm *.dmp
		cp /d/$DMP_FOLDER/*.dmp .

	else
		usage
    fi
	
elif [ "$1" == "build" ]; then
	rm *.zip
	make distclean ATMOSPHERE=1 || exit 1
	
elif [ "$1" == "stacktrace" ]; then
    echo "PC: XXXXXXX value ?"
    read pc_value
	
    echo "Backtrace - Start Address: XXXXXXX value ?"
    read backtrace_value

    addr=$(printf "0x%X\n" $((0x$pc_value - 0x$backtrace_value)))
    /opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line.exe -e "$ELF_FILE" -f -p -C -a "$addr"
else
	usage
fi
