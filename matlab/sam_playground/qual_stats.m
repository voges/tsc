function status = qual_stats(fn)
%QUAL_STATS Generate statistics for a ".sam.qual" file
%
%   status = QUAL_STATS(fn)
%
%   Input : fn     - File name
%   Output: status - Returns 1 on success, otherwise 0

    block_sz = 1000;                    %< number of lines per block
    block_n = 10;                       %< number of blocks
    fid = fopen([fn,'.qual'],'r'); %< open file

    for b = 1:block_n
        fprintf('Block %d/%d ...\n',b,block_n);
        Q = tntlib_read_ascii(fid,block_sz);

        % Write the quality score lines to a matrix M. The lines might have
        % different lengths. Short lines are being filled with trailing NaN's.
        [max_line_length,~] = max(cellfun(@numel,Q));
        M = zeros(block_sz,max_line_length) .* nan;
        l = zeros(block_sz,1);
        for i = 1:block_sz
            l(i) = length(Q{i,1});
            M(i,1:l(i)) = Q{i,1};
        end

        % Write the quality scores row-wise to a stream q_row, remove NaNs.
        q_row = reshape(M',1,size(M,1) * size(M,2));
        q_row = q_row(~isnan(q_row));

        % Write the quality scores column-wise to a stream q_col, remove
        % NaNs.
        q_col = reshape(M,1,size(M,1) * size(M,2));
        q_col = q_col(~isnan(q_col));
        
        % Plot results
        figure(1);

        subplot(2,4,1); imagesc(M);

        [x,h] = tntlib_integer_histogram(q_row);
        subplot(2,2,2); bar(x,h);
        title('Histogram');
        xlabel('Symbol value');
        ylabel('Absolute frequency');

        [acorr,lags] = xcorr(q_row,'coeff');
        subplot(2,2,3); plot(lags,acorr); grid;
        title('Row-wise Autocorrelation');
        xlabel('Lags');
        ylabel('Normalized value');

        [acorr,lags] = xcorr(q_col,'coeff');
        subplot(2,2,4); plot(lags,acorr); grid;
        title('Column-wise Autocorrelation');
        xlabel('Lags');
        ylabel('Normalized value');
    end
    
    fclose(fid);
    status = 1;
end

