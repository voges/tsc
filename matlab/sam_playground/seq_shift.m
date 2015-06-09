%
% TODO
% - Display block-wise sequences (w/ NaN padding)
% - Sometimes won't decode!
%

function status = seq_shift(fn)
%SEQ_SHIFT Implements the sequence shifting algorithm
%
%   status = SEQ_SHIFT(fn)
%
%   Input : fn     - File name
%   Output: status - Returns 1 on success, otherwise 0

    block_sz = 10000;                   %< number of lines per block
    block_n = 10;                     %< number of blocks
    fid_pos = fopen([fn,'.pos'],'r'); %< open file 
    fid_seq = fopen([fn,'.seq'],'r'); %< open file

    for b = 1:block_n
        fprintf('Block %d/%d ...\n',b,block_n);
        
        % Read sequences into a cell array S and position into cell array P
        Seq = tntlib_read_ascii(fid_seq,block_sz);
        Pos = tntlib_read_ascii(fid_pos,block_sz);
        
        % Write the sequences to a matrix S. The lines might have
        % different lengths. Short lines are being filled with trailing NaN's.
        [max_line_length,~] = max(cellfun(@numel,Seq));
        S = zeros(block_sz,max_line_length) .* nan;
        l = zeros(block_sz,1);
        for i = 1:block_sz
            l(i) = length(Seq{i,1});
            S(i,1:l(i)) = Seq{i,1};
        end

        % Write the sequences row-wise to a stream s, remove NaNs.
        s = reshape(S',1,size(S,1) * size(S,2));
        s = s(~isnan(s));
        
        % Write positions to an integer stream p
        p = zeros(block_sz,1);
        for i = 1:block_sz
            digits = (Pos{i} - 48); %< ASCII offset = 48
            p(i) = tntlib_dig2int(digits);
        end
        
        % Visualize positions and sequences
        figure(1);
        subplot(1,2,1); scatter(1:length(p),p);
        title(['File: ',fn,' Block: ',int2str(b)]);
        xlabel('Line number');
        ylabel('Position');
        subplot(1,2,2); imagesc(S);
        
        % Write first line to rstream
        r = [];
        c = [];
        r = s(1:l(1));
        s_ind = l(1) + 1;
        r_ind = 1;
        r_ind_start = r_ind; %!!!
        
        for line = 2:block_sz
            p_off = p(line) - p(line - 1)
            match_len = min(l(line - 1) - p_off,l(line));
            % TODO: what to do in case of no match (match_len < 0)?
            
            if (match_len > 0)
                s_ind_start = s_ind;
                s_ind_end = s_ind_start + match_len - 1;
                r_ind_start = r_ind_start + p_off;%!!!
                r_ind_end = r_ind_start + match_len - 1;

                e = s(s_ind_start:s_ind_end) - r(r_ind_start:r_ind_end);
                c = [c,e];

                if p_off > 0
                    r = [r,s(s_ind_end + 1:s_ind_start + l(line) - 1)];
                end

                s_ind = s_ind + l(line);
            else
                fprintf('skipping line\n');
            end
        end

        
        % Entropies
        fprintf('Block entropies:\n');
        fprintf('ASCII    : %8.8f bit/symbol, total: %8.8f bits\n',8,8*length(s));
        fprintf('s        : %8.8f bit/symbol, total: %8.8f bits\n',tntlib_entropy(s),tntlib_entropy(s)*length(s));
        fprintf('c        : %8.8f bit/symbol, total: %8.8f bits\n',tntlib_entropy(c),tntlib_entropy(c)*length(c));
        fprintf('r        : %8.8f bit/symbol, total: %8.8f bits\n',tntlib_entropy(r),tntlib_entropy(r)*length(r));
        
        
    end
    fclose(fid_seq);
    fclose(fid_pos);
    
    fprintf('File entropies:\n');

end

