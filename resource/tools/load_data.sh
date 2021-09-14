while read -r line
do
doc_id=`echo "$line"| jq '.table_content.fields.doc_id'`
curl -X POST http://127.0.0.1/insert -H 'content-type: application/json' -H 'doc_id:'"$doc_id" -d "$line"
echo 'doc_id:'"$doc_id"
echo $line
done < send.json
