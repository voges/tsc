function [x,h] = tntlib_integer_histogram(S)
%TNTLIB_INTEGER_HISTOGRAM - Compute integer histogram of a given sequence S
%
%   [x,h] = TNTLIB_INTEGER_HISTOGRAM(S)
%
%   Input : S - Sequence as (m,n) matrix containing integer values
%   Output: x - Row vector containing the bins (a.k.a. integers)
%           h - Row vector containing the relative frequencies of each bin

    minimum = min(min(S));
    maximum = max(max(S));
    x = minimum:maximum;
    h = zeros(1,maximum-minimum+1);
    
    for i = 1:numel(S)
        h(S(i)-minimum+1) = h(S(i)-minimum+1) + 1;
    end
end

