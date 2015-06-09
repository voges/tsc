function n = tntlib_rice_decode(c,p)
%TNTLIB_RICE_DECODE - Decode a binary Rice code c with Rice parameter p
%
%   n = TNTLIB_RICE_DECODE(c,p)
%
%   Input : c - Row vector containing binary Rice code
%           p - Rice parameter used for encoding
%   Output: n - Decoded decimal number

    % Decode unary part (count ones)
    q = 0;
    i = 1;
    while (c(i) == 1)
        q = q + 1;
        i = i + 1;
    end
    q = q * p;
    
    % Remainder
    r = bi2de(fliplr(c(i+1:length(c))));
    
    % Put together the decoded decimal
    n = q + r;
end

