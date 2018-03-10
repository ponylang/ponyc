#!/usr/bin/env bash
# Copy tags from a git repo at url to the
# local copy of that repo adding a prefix to
# each tag.

usage() {
	echo "Copy tags from remote repo to the local repo"
	echo "  Usage: $0 url=xxx prefix=yyy {regex=zzz}"
	echo "    --url=xxx    xxx is the url of the remote repo"
	echo "    --prefix=yyy yyy is the prefix to add to the tag" 
	echo "    --regex=yyy  yyy is the regular expression to match tags"
	echo "                 yyy maybe \"*\", \"all\" or empty to match all tags"
	echo "    --help       Displays this help message"
	echo
	echo "  A parameter in braces is optional"
       	exit 1
}

# Create a tag if it hasn't already been created
#
# $1 = prefix
# $2 = tag to create
# $3 = sha1
create_tag_prev=
create_tag_not_previously_created () {
	#echo "1='$1' 2='$2' 3='$3'"
	if test "create_tag_prev" != "$2"; then
	       	git tag $1$2 $3 >>/dev/null 2>&1
		test "$?" != "0" && return 1
		echo "Made tag '$1$2' (at $3)"
		create_tag_prev=$2
	fi
}

IFS=' ' read -r -a params <<< "${@}"
#echo "params: ${params[@]}"

dsp_usage=""
url=""
prefix=""
regex=""

for name_value in "${params[@]}"; do
	#echo "name_value: ${name_value}"
	IFS="=" read -r name value <<< "${name_value}"
	#echo "name=${name} value=${value}"
	case "${name}" in
		/?|-?|?|-h|--help|help)
			#echo "help value='${value}'"
			dsp_usage="true"
			;;
		--url|url)
			#echo "url='${value}'"
			url=${value}
			;;
		--prefix|prefix)
			#echo "prefix='${value}'"
			prefix=${value}
			;;
		--regex|regex)
			#echo "regex='${value}'"
			regex=${value}
			;;
		*)
			echo "unknown: name=${name} value='${value}'"
			dsp_usage="true"
			;;
	esac
done
#echo "url=${url} prefix=${prefix} regex=${regex}"

if [[ "${url}" == "" || "${dsp_usage}" == "true" ]]; then
	usage
       	exit 1
fi

if [[ "${prefix}" == "" ]]; then
	echo "Missing prefix, aborting!"
	usage
	exit 1
fi

if [[ "${regex}" = "all" || "${regex}" == "*" ]]; then
	regex=""
fi

# Read the output of git ls-remote with one line per array
IFS=$'\n' tags=( $(git ls-remote --tags ${url}) )

count=0
for element in "${tags[@]}"; do
	#echo "${element}"
	line=( $(echo "${element}" | sed -e "s/[[:space:]]\+/ /g") )
	#echo "line=${line}"
	IFS=' ' read -r sha1 ref <<< "${line}"
	#echo "sha1=${sha1} ref=${ref}"
	IFS='/' read -r _ _ tag <<< "${ref}"
	#echo "tag=${tag}"

	# Test that the tag is NOT empty and matches the regex if we have a regex
	if [[ "${tag}" != "" && ( "${regex}" == "" || "${tag}" =~ ${regex} ) ]]; then
		if [[ ${tag} != *"^{}" ]]; then
			# This is a pure light-weight tag and may or may not
			# reference a thing. If it does we're good and the
			# tag will be created.
			create_tag_not_previously_created ${prefix} ${tag} ${sha1}
			test "$?" = "0" && count=$((count+1))
		else
			# traiing "^{}" exists, remove it and try to create tag
			lw_tag="${tag%%\^\{\}}"
			create_tag_not_previously_created ${prefix} ${lw_tag} ${sha1}
			test "$?" = "0" && count=$((count+1))
		fi
	fi
done
test "$count" = "0" && echo "No tags copied"
