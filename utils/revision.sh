#!/bin/sh
USAGE="$(basename "$0")
    [-g/--get get current version]
    [--major bump major]
    [--minor bump minor]
    [--patch bump patch]
    [--rev   bump revision]
    [NOTE: this script must be exectued within doormat git folder]"

VERSION=$(git describe --match "v[0-9]*.[0-9]*.[0-9]*.[0-9]*" --abbrev=0|cut -b 2-)
MAJOR=$(echo "$VERSION"|cut -d'.' -f1)
MINOR=$(echo "$VERSION"|cut -d'.' -f2)
PATCH=$(echo "$VERSION"|cut -d'.' -f3)
REV=$(echo "$VERSION"|cut -d'.' -f4)

case $1 in
	-g | --get ) 
		echo "${VERSION}"
		exit 0 
		;;

	--major )
		MAJOR=$((MAJOR+1))
	MINOR=0
	PATCH=0
	REV=0
		#echo "Bumping Major Version Number --> ${MAJOR} (Do not forget to push new tag to remote!)"
		echo "${MAJOR}.${MINOR}.${PATCH}.${REV}"
	git tag -a "v${MAJOR}.${MINOR}.${PATCH}.${REV}" -m "Bumping Major Version Number"
		exit $! 
		;;
		
	--minor )
		MINOR=$((MINOR+1))
	PATCH=0
	REV=0
		#echo "Bumping Minor Version Number --> ${MINOR} (Do not forget to push new tag to remote!)"
		echo "${MAJOR}.${MINOR}.${PATCH}.${REV}"
		git tag -a "v${MAJOR}.${MINOR}.${PATCH}.${REV}" -m "Bumping Minor Version Number"
		exit $! 
		;;
		
	--patch )
		PATCH=$((PATCH+1))
	REV=0
		#echo "Bumping Patch Version Number --> ${PATCH} (Do not forget to push new tag to remote!)"
		echo "${MAJOR}.${MINOR}.${PATCH}.${REV}"
		git tag -a "v${MAJOR}.${MINOR}.${PATCH}.${REV}" -m "Bumping Patch Version Number"
		exit $! 
		;;
		
	--rev )
		REV=$((REV+1))
		#echo "Bumping Revision --> ${REV} (Do not forget to push new tag to remote!)"
		echo "${MAJOR}.${MINOR}.${PATCH}.${REV}"
		git tag -a "v${MAJOR}.${MINOR}.${PATCH}.${REV}" -m "Bump Revision"
		exit $! 
		;;
	 *)
                echo "${USAGE}"
                exit 1
                ;;
esac
