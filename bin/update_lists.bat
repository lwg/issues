copy /y lwg-*.html ..\issues-gh-pages
copy /y unresolved-*.html ..\issues-gh-pages
copy /y votable-*.html ..\issues-gh-pages
pushd ..\issues-gh-pages
call git commit -a -m"Update"
call git push  "origin" gh-pages:gh-pages
popd
