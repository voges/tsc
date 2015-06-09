function lc = tntlib_lc(fn)
%TNTLIB_LC - Count the number of lines in given file
%
%   lc = TNTLIB_LC(fn)
%
%   Input : fn - Filename
%   Output: lc - Number of lines in fn. Returns 0 if file is not readable.

    fid = fopen(fn,'r');
    if fid < 0
        lc = 0;
    else
        lc = 0;
        while 1
            ln = fgetl(fid);
            if ~ischar(ln)
                break;
            end
            lc = lc + 1;
        end;
        fclose(fid);
    end
end

