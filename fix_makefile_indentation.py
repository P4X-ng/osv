#!/usr/bin/env python3

import re

def fix_makefile_indentation(filepath):
    """Fix Makefile indentation by converting spaces to tabs for recipe lines."""
    
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    fixed_lines = []
    in_recipe = False
    
    for i, line in enumerate(lines):
        # Check if this line is a target (ends with :)
        if line.strip() and not line.startswith('#') and not line.startswith('\t') and ':' in line and not line.strip().startswith('ifeq') and not line.strip().startswith('ifneq') and not line.strip().startswith('ifdef') and not line.strip().startswith('ifndef') and not line.strip().startswith('else') and not line.strip().startswith('endif'):
            # This might be a target line
            if line.rstrip().endswith(':') or ('=' not in line.split(':')[0]):
                in_recipe = True
                fixed_lines.append(line)
                continue
        
        # Check if we're no longer in a recipe (non-indented line that's not empty/comment)
        if line.strip() and not line.startswith('\t') and not line.startswith(' ') and not line.startswith('#'):
            in_recipe = False
        
        # If this line starts with spaces and we might be in a recipe, convert to tab
        if line.startswith('        ') and not line.startswith('         '):
            # This line starts with exactly 8 spaces, likely a recipe line
            fixed_line = '\t' + line[8:]
            fixed_lines.append(fixed_line)
        elif line.startswith('    ') and not line.startswith('     ') and in_recipe:
            # This line starts with exactly 4 spaces in a recipe context
            fixed_line = '\t' + line[4:]
            fixed_lines.append(fixed_line)
        else:
            fixed_lines.append(line)
    
    # Write the fixed content back
    with open(filepath, 'w') as f:
        f.writelines(fixed_lines)
    
    print(f"Fixed Makefile indentation in {filepath}")

if __name__ == "__main__":
    fix_makefile_indentation('/workspace/Makefile')