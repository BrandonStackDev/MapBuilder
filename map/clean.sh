#!/bin/bash

echo "Cleaning chunk folders..."
pwd
ls -la chunk_*
echo "WARNING: This will delete all chunk folders. Continue? (y/n)"
read -r confirm

if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]; then
    echo "Deleting chunk folders..."
    rm -rfv chunk_*
    echo "Done."
else
    echo "Aborted."
fi