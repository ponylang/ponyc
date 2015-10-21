DATE=`date +%Y-%m-%d`

YAML="{
  \"package\": {
    \"name\": \"ponyc\",
    \"repo\": \"$1\", 
    \"subject\": \"ponylang\"
  },
  \"version\": {
    \"name\": \"$2\",
    \"desc\": \"ponyc release $2\",
    \"released\": \"$DATE\",
    \"vcs_tag\": \"$2\",
    \"gpgSign\": true
  },"

case $1 in
  "debian")
    FILES="\"files\":
        [
          {
            \"includePattern\": \"build/bin/(.*\.deb)\", \"uploadPattern\": \"\$1\",
            \"matrixParams\": {
            \"deb_distribution\": \"vivid\",
            \"deb_component\": \"main\",
            \"deb_architecture\": \"amd64\"}
         }
       ],
       \"publish\": true" 
    ;;
  "rpm")
    FILES="\"files\":
      [
        {\"includePattern\": \"build/bin/(.*\.rpm)\", \"uploadPattern\": \"\$1\"}
      ],
    \"publish\": true" 
    ;;
  "source")
    FILES="\"files\":
      [
        {\"includePattern\": \"build/bin/(.*\.tar.bz2)\", \"uploadPattern\": \"\$1\"}
      ],
    \"publish\": true"
    ;;
esac

YAML=$YAML$FILES"}"  

echo $YAML >> bintray_$1.yml
