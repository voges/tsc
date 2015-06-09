function Q = tntlib_read_ascii(fid,n)
%TNTLIB_READ_ASCII - Read n lines from an ASCII file
%
%   Q = TNTLIB_READ_ASCII(fid,n)
%
%   Input : fid - File ID (take care of opening/closing the file)
%           n   - Number of rows to store in Q
%   Output: Q   - Cell array containing the n rows

    Q = cell(n,1);
    line_cnt = 1;
    
    while ~feof(fid) && line_cnt <= n
        line = fgetl(fid);
        if isempty(line) 
            break;
        end
        Q{line_cnt,1} = double(line);
        line_cnt = line_cnt + 1;
    end
end

