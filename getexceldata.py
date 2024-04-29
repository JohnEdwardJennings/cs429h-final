import re

# Read the output from a file or provide it as a string
with open('output.txt', 'r') as file:
    output = file.read()

# Define a regular expression pattern to match the lines with numbers
pattern = r'^(\w+)_(\d+)B\.txt:\s*(\d+\.\d+)\s*\|'

# Iterate through the lines and extract the file name
for line in output.splitlines():
    words = line.split()
    first_word = words[0]
    if len(words) > 1:
        second_word = words[1]
        print(second_word)
    else:
        print(" ")
