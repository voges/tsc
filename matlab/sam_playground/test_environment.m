% Add path to tntlib
addpath([getenv('GENOME_COMPRESSION'),'/matlab/tntlib']);

% Get list of short SAM files
f_list = fopen([getenv('GENOME_COMPRESSION'),'/gidb/sam.short.sam.list'],'r');
f_names = textscan(f_list,'%s');
fclose(f_list);
f_n = size(f_names{1},1);

for f = 1:f_n
    fn = f_names{1}{f};
    fprintf('File: %s\n',fn);
    %fprintf('qual_markovfir\n');
    %qual_markovfir(fn)
    %fprintf('qual_markovpredictorcomplex\n');
    %qual_markovpredictorcomplex(fn);
    fprintf('qual_markovpredictorsimple\n');
    qual_markovpredictorsimple(fn);
    %fprintf('qual_stats\n');
    %qual_stats(fn);
    %fprintf('seq_shift\n');
    %seq_shift(fn);
end

% Different read lengths
%fn = '/data/genome/human/IonTorrent/tmp/short/sample-2-10_sorted.sam';

% Low coverage
%fn = '/data/genome/human/illumina/ERR317482WGS/tmp/short/9827_2#49.sam';

% Shifted
%sam_fn = '/data/genome/bacteria/DH10B/tmp/short/MiSeq_Ecoli_DH10B_110721_PF.sam';

