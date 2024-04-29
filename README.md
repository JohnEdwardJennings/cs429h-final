# cs429h-final
An environment for testing and comparing branch predictors. Uses the ChampSim CPU simulator, written in C++, with the IPC1 benchmarks.

Team members:
- Tejas Oberoi
- David Lee
- Sahil Chowdhury
- John Jennings

## Instructions

### Setup (for UTCS lab machines)
1. Set up a GitHub personal access token.
	1. Go to [https://github.com/settings/tokens/new](https://github.com/settings/tokens/new). 
	2. Under "Expiration", select any duration that lasts through the project deadline (you will always be able to revoke the token later). 
	3. Under "Select scopes", check only the box marked "repo".
	4. Click "Generate token".
	5. Save the token somewhere accessible; you will enter it every time git asks for a password.

2. Install `jq`, a utility that the testing script uses to edit JSON configuration files:
```
ssh linux.cs
curl -s https://webinstall.dev/jq | bash
```
If prompted, enter:
```
source ~/.config/envman/PATH.env
```

3. Clone the repository:
```
git clone --recurse-submodules https://github.com/JohnEdwardJennings/cs429h-final.git cs429h-final
```
Enter your personal access token when prompted for a password.

4. Resolve ChampSim's dependencies:
```
cd ChampSim
git submodule update --init
vcpkg/bootstrap-vcpkg.sh
vcpkg/vcpkg install
cd ..
```

5. Copy the benchmarks (these files are too large for GitHub):
	1. Go to [https://drive.google.com/file/d/1qs8t8-YWc7lLoYbjbH_d3lf1xdoYBznf/view?pli=1](https://drive.google.com/file/d/1qs8t8-YWc7lLoYbjbH_d3lf1xdoYBznf/view?pli=1).
	2. Click "Download". Google Drive will display a warning about the file being too large to scan for viruses; click "Download anyway".
	3. Run the following commands, substituting your downloads folder for `<Downloads>` and your UTCS username for `<your-csid>`. The `scp` command must execute from your machine, hence exiting and re-entering the SSH connection.
```
exit
cd <Downloads>
scp ipc1_public.tar.gz <your-csid>@linux.cs.utexas.edu:cs429h-final
ssh linux.cs
cd cs429h-final
tar xvzf ipc1_public.tar.gz -C ./
rm ipc1_public.tar.gz
```

### Add a branch predictor
```
mkdir branch-predictors/<branch-predictor-name>
cp interface.txt branch-predictors/<branch-predictor-name>/<filename>.cc
vim branch-predictors/<branch-predictor-name>/<filename>.cc
```
* Substitute desired strings for `<branch-predictor-name>` and `<filename>` in the commands above (to be safe, stick to alphanumeric characters and hyphens).
* Implement the methods to create a fully functional branch predictor!

### Run tests (IPC1 benchmarks)
```
. bptest
```

The output for each test will appear in the file `out/<branch-predictor-name>_<test_name>`.

To run only a specific branch predictor:
```
. bptest <branch-predictor-name>
```


### Running spec2006
To run against spec, enter the Champsim directory, do 

```
./build.sh TAGE-SC-L lru 1
```
(you may replace TAGE-SC-L with the type of branch predictor).

On a separate terminal, ssh into the darmok server, and run the script
```
./run_spec2006.sh TAGE-SC-L lru
```
which will submit results to the cluster. The results will appear in the output directory in Champsim.
