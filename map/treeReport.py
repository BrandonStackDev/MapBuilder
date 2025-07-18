import os
import argparse

# Parse command-line arguments
parser = argparse.ArgumentParser()
parser.add_argument('-z', '--hide-zero', action='store_true', help='Hide chunks with 0 trees')
parser.add_argument('-c', '--cap', action='store_true', help='Cap tree counts at 512 and write back')
args = parser.parse_args()

found_any = False

for folder in os.listdir("."):
    if folder.startswith("chunk_") and os.path.isdir(folder):
        found_any = True
        tree_file_path = os.path.join(folder, "trees.txt")
        try:
            with open(tree_file_path, "r") as f:
                first_line = f.readline().strip()
                num_trees = int(first_line)

            if args.cap and num_trees > 512:
                # Overwrite the file with the capped value
                with open(tree_file_path, "w") as f:
                    f.write("512\n")
                num_trees = 512

            if args.hide_zero and num_trees == 0:
                continue

            print(f"{folder}: {num_trees} trees")

        except FileNotFoundError:
            print(f"{folder}: trees.txt not found")
        except ValueError:
            print(f"{folder}: invalid number format in trees.txt")
        except Exception as e:
            print(f"{folder}: error reading trees.txt — {e}")

if not found_any:
    print("No chunk_* folders found.")
