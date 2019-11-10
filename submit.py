#!/usr/bin/python
import time
import os
import sys
import argparse
import json
import datetime
import hashlib
import glob
import shutil
from nelson.gtomscs import submit

CKSUM_FILE_NAME = "sum19-cksum.txt"


def cleanup_cksum():
  try:
      os.remove(CKSUM_FILE_NAME)
  except OSError:
      pass

def cleanup_student_zip():
  try:
    os.remove('student.zip')
  except OSError:
    pass

# from https://stackoverflow.com/questions/3431825/generating-an-md5-checksum-of-a-file
def hash_bytestr_iter(bytesiter, hasher, ashexstr=False):
    for block in bytesiter:
        hasher.update(block)
    return (hasher.hexdigest() if ashexstr else hasher.digest())

def file_as_blockiter(afile, blocksize=65536):
  with afile:
      block = afile.read(blocksize)
      while len(block) > 0:
          yield block
          block = afile.read(blocksize)

def compute_cksum(file_list):
  cksum = {}
  for file in file_list:
    if os.path.isdir(file): continue # skip directories
    try:
      stat = os.stat(file)
    except OSError:
      continue
    if (stat.st_size > 1024 * 1024): continue
    cksum[file] = hash_bytestr_iter(file_as_blockiter(open(file, 'rb')), hashlib.sha256(), True)
  return cksum

def get_submit_cksum(quiz=""):
    file_list = glob.glob("*")
    if quiz != "": file_list += glob.glob("../*")
    cksum = compute_cksum(file_list)
    with open(CKSUM_FILE_NAME, 'w+') as f:
        for key, value in cksum.items():
            f.write('%64.64s  %s\n' % (key, value))

def compute_readme_list():
    readme_list = []
    readme_candidates = ['readme-student.md', 'readme-student.pdf']
    for candidate in readme_candidates:
        if os.path.isfile(candidate):
            readme_list.append(candidate)
    if len(readme_list) is 0:
        raise Exception("There is no valid student readme file to submit")
    return readme_list

def main():
  parser = argparse.ArgumentParser(description='Submits code to the Udacity site.')
  parser.add_argument('quiz', choices = ['part1', 'part2', 'readme'])

  args = parser.parse_args()

  path_map = {'part1': 'part1',
              'part2': 'part2',
              'readme': '.'}

  quiz_map = {'part1': 'pr4_grpc',
              'part2': 'pr4_grpc_dfs',
              'readme': 'pr4_readme' }

  files_map = {
          'pr4_grpc': ['dfslib-clientnode-p1.cpp',
                       'dfslib-clientnode-p1.h',
                       'dfslib-servernode-p1.cpp',
                       'dfslib-servernode-p1.h',
                       'dfslib-shared-p1.cpp',
                       'dfslib-shared-p1.h',
                       'dfs-service.proto'],
          'pr4_grpc_dfs': ['dfslib-clientnode-p2.cpp',
                           'dfslib-clientnode-p2.h',
                           'dfslib-servernode-p2.cpp',
                           'dfslib-servernode-p2.h',
                           'dfslib-shared-p2.cpp',
                           'dfslib-shared-p2.h',
                           'dfs-service.proto'],
          'pr4_readme': compute_readme_list()}

  # Append the proto file
  quiz = quiz_map[args.quiz]
  cleanup_cksum()

  os.chdir(path_map[args.quiz])

  try:
    get_submit_cksum(quiz)
  except:
    pass # ignore errors
  if (os.path.exists(CKSUM_FILE_NAME)): files_map[quiz].append(CKSUM_FILE_NAME)

  submit('cs6200', quiz, files_map[quiz])

  cleanup_cksum()
  cleanup_student_zip()


if __name__ == '__main__':
  main()

