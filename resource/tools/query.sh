while read -r line
do
echo $line
curl -X POST http://127.0.0.1/search -H 'content-type: application/json' -d "$line"
done < search.json
